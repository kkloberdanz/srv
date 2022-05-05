# srv

srv is a very simple HTTP server written entirely in C. It is not intended to be
useful. I wrote this program for three reasons.

1. To benchmark slow Python webservers vs a pure C server.
2. To test out how well my thread pool implementation would work, i.e., `wrkq.c`.
3. Because I was bored and this seemed like fun.

On startup, a thread pool is allocated using my "work queue" implementation,
`wrkq.c`. Each request gets enqueued to this work queue and executed on a
thread. The benefit of this design is that because the threads are allocated on
initial startup, requests do not have to wait for a thread to spawn, which can
be quite slow. Threads are then re-used after the task has been completed.
