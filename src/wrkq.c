#include <stdio.h>
#include <stdlib.h>

#include <wrkq.h>

struct wrkq_t {
    struct wrkq_job *queue;
    struct wrkq_result *results;
    pthread_t *threads;
    size_t n_workers;
    size_t queue_depth;
    size_t queue_consumer_index;
    size_t queue_producer_index;
    size_t result_index;
    size_t id;
    size_t jobs_finished;
    pthread_mutex_t mtx;
    sem_t empty_count;
    sem_t fill_count;
};

static void pull_job(struct wrkq_t *q, struct wrkq_job *out);
static void push_result(struct wrkq_t *q, struct wrkq_result *result);

static void *wrkq_thread_loop(void *args) {
    struct wrkq_t *q = args;
    struct wrkq_job work_job;
    struct wrkq_result result;
    void *return_data = NULL;

    for (;;) {
        pull_job(q, &work_job);
        return_data = work_job.func(work_job.arg);
        result.value = return_data;
        result.id = work_job.id;
        push_result(q, &result);

        pthread_mutex_lock(&q->mtx);
        q->jobs_finished++;
        pthread_mutex_unlock(&q->mtx);
    }
    return NULL;
}

static int init_locks(struct wrkq_t *q) {
    int err = 0;

    err = pthread_mutex_init(&q->mtx, NULL);
    if (err) {
        perror("wrkq: failed to initialize mutex");
        goto error;
    }

    err = sem_init(&q->empty_count, 0, q->queue_depth);
    if (err) {
        perror("wrkq: failed to initialize semaphore");
        goto destroy_empty_count;
    }

    err = sem_init(&q->fill_count, 0, 0);
    if (err) {
        perror("wrkq: failed to initialize semaphore");
        goto destroy_fill_count;
    }

    return 0;

destroy_fill_count:
    sem_destroy(&q->fill_count);
destroy_empty_count:
    sem_destroy(&q->empty_count);
error:
    pthread_mutex_destroy(&q->mtx);
    return 1;
}

struct wrkq_t *wrkq_new(struct wrkq_options *opt) {
    struct wrkq_job *queue = NULL;
    struct wrkq_t *q = NULL;
    pthread_t *threads = NULL;
    size_t n_started = 0;
    size_t i = 0;

    q = malloc(sizeof(*q));
    if (!q) {
        goto fail;
    }
    q->n_workers = opt->n_workers;
    q->queue_depth = opt->queue_depth;
    q->id = 0;
    q->jobs_finished = 0;

    queue = malloc(sizeof(*queue) * opt->queue_depth);
    if (!queue) {
        goto fail;
    }
    q->queue = queue;
    q->queue_consumer_index = 0;
    q->queue_producer_index = 0;

    threads = calloc(opt->n_workers, sizeof(*threads));
    if (!threads) {
        goto fail;
    }
    q->threads = threads;

    if (init_locks(q)) {
        goto fail;
    }
    for (n_started = 0; n_started < opt->n_workers; n_started++) {
        int status = pthread_create(
            &threads[n_started],
            NULL,
            wrkq_thread_loop,
            q
        );
        if (status) {
            perror("wrkq: failed to create thread");
            goto fail;
        } else {
            pthread_detach(threads[n_started]);
        }
    }
    
    return q;

fail:
    free(q);
    free(queue);
    for (i = 0; i < n_started; i++) {
        pthread_cancel(threads[i]);
    }
    free(threads);
    return NULL;
}

void wrkq_destroy(struct wrkq_t *q) {
    size_t i;
    for (i = 0; i < q->n_workers; i++) {
        pthread_cancel(q->threads[i]);
    }
    sem_destroy(&q->empty_count);
    sem_destroy(&q->fill_count);
    pthread_mutex_destroy(&q->mtx);
    free(q->queue);
    free(q->threads);
    free(q);
}

/*
 * Producer-consumer model
 *
 * The locking gets pretty hairy here, best to use an existing algorithm
 * that is known to work (see multi producer/consumer example)
 * https://en.wikipedia.org/wiki/Producer%E2%80%93consumer_problem#Using_semaphores
 */
static size_t job_enqueue(struct wrkq_t *q, struct wrkq_job *job) {
    q->queue[q->queue_producer_index] = *job;
    q->queue_producer_index = (q->queue_producer_index + 1) % q->queue_depth;
    q->id++;
    q->queue[q->queue_producer_index].id = q->id;
    return q->id;
}

static void job_dequeue(struct wrkq_t *q, struct wrkq_job *out) {
    *out = q->queue[q->queue_consumer_index];
    q->queue_consumer_index = (q->queue_consumer_index + 1) % q->queue_depth;
}

size_t wrkq_nq(struct wrkq_t *q, struct wrkq_job job) {
    size_t id = 0;

    sem_wait(&q->empty_count);
    pthread_mutex_lock(&q->mtx);
    id = job_enqueue(q, &job);
    pthread_mutex_unlock(&q->mtx);
    sem_post(&q->fill_count);
    return id;
}

static void pull_job(struct wrkq_t *q, struct wrkq_job *out) {
    sem_wait(&q->fill_count);
    pthread_mutex_lock(&q->mtx);
    job_dequeue(q, out);
    pthread_mutex_unlock(&q->mtx);
    sem_post(&q->empty_count);
}

static void push_result(struct wrkq_t *q, struct wrkq_result *result) {
#define UNUSED(A) (void)(A)
    UNUSED(q);
    UNUSED(result);
#undef UNUSED
    /* TODO: put result to the results queue */
}

void wrkq_dq(struct wrkq_t *q, struct wrkq_result *result) {
    /* TODO: wait on results queue for a result to come in, then return
     * the first possible result to the result pointer
     */
#define UNUSED(A) (void)(A)
    UNUSED(q);
    UNUSED(result);
#undef UNUSED
    if (result) {

    } else {

    }
}

void wrkq_join(struct wrkq_t *q, struct wrkq_result **results) {
    size_t difference;
    size_t i = 0;
    const char *msg = \
        "wrkq: unable to allocate memory for results, joining without data";

    pthread_mutex_lock(&q->mtx);
    difference = q->jobs_finished - q->id;
    pthread_mutex_unlock(&q->mtx);

    if (results) {
        *results = malloc(sizeof(struct wrkq_result) * difference);
        if (!*results) {
            perror(msg);
        }
    }

    for (i = 0; i < difference; i++) {
        if (results && *results) {
            wrkq_dq(q, &*results[i]);
        } else {
            wrkq_dq(q, NULL);
        }
    }
}
