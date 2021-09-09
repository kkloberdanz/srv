#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/sendfile.h>
#include <sys/mman.h>
#include <linux/mman.h>

#include "wrkq.h"

#define PORT 8080

#define UNUSED(A) (void)(A)

union void_ptr_to_int {
    void *vp;
    int i;
};

const char *response = "HTTP/1.1 200 OK\r\n"
    "Content-Length: 122\r\n\r\n"
    "<!DOCTYPE html>"
        "<html lang=\"en\">"
          "<head>"
            "<meta charset=\"utf-8\">"
            "<title>Hello!</title>"
          "</head>"
          "<body>"
            "<h1>Hello!</h1>"
          "</body>"
        "</html>"
    "\r\n\r\n";
size_t sz = 0;

void writeall(int fd, const void *buf, size_t count) {
    while (count > 0) {
        count -= write(fd, buf, count);
    }
}

void readall(int fd, char *buf, size_t count) {
    ssize_t bytes_read = 0;
    while (count > 0) {
        bytes_read = read(fd, buf, count);
        if (bytes_read) {
            count -= bytes_read;
        } else {
            return;
        }
    }
}

void handle_request(int new_socket) {
    char buf[1024] = {0};
    const size_t buf_sz = 1024;
    ssize_t bytes_read;

    bytes_read = read(new_socket, buf, buf_sz - 1);
    UNUSED(bytes_read);

    writeall(new_socket, response, sz);
    close(new_socket);
}

void *thread_start(void *arg) {
    union void_ptr_to_int vp_to_i;

    vp_to_i.vp = arg;
    handle_request(vp_to_i.i);
    return NULL;
}

int main(void) {
    int server_fd;
    int new_socket;
    struct sockaddr_in address;
    int opt = 1;
    size_t addrlen = sizeof(address);
    struct wrkq_t *q = NULL;
    struct wrkq_options q_opt;

    signal(SIGCHLD, SIG_IGN);

    sz = strlen(response);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) <= 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (
        setsockopt(
            server_fd,
            SOL_SOCKET,
            SO_REUSEADDR | SO_REUSEPORT,
            &opt,
            sizeof(opt)
        )
    ) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    q_opt.n_workers = 16;
    q_opt.queue_depth = 255;
    q = wrkq_new(&q_opt);

    /* infinite loop */
    puts("listening\n");
    for (;;) {
        struct wrkq_job job;
        if ((
            new_socket = accept(
                server_fd,
                (struct sockaddr *)&address,
                (socklen_t *)&addrlen
            )
        ) < 0) {
            perror("accept");
        }

        union void_ptr_to_int vp_to_i;
        vp_to_i.i = new_socket;

        job.func = thread_start;
        job.arg = vp_to_i.vp;
        wrkq_nq(q, job);
    }
}
