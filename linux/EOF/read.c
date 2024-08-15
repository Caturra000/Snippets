#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>

// https://stackoverflow.com/questions/21868812/why-does-a-call-to-read-block-forever
// 常规文件没这种问题

volatile sig_atomic_t fds[2];

void fail(const char *reason) {
    perror(reason);
    abort();
}

void sigint(int) {
    close(fds[0]);
    close(fds[1]);
    unlink("temp");
    exit(EXIT_SUCCESS);
}

void* async_worker_func(void*) {
    int fd = open("temp", O_WRONLY);
    if(fd == -1) fail("fd(2)");
    fds[1] = fd;
    write(fd, "x", 1);
    return NULL;
}

int main() {
    int fd = open("temp", O_CREAT | O_RDONLY);
    if(fd == -1) fail("fd");
    fds[0] = fd;

    pthread_t worker;
    pthread_create(&worker, NULL, async_worker_func, NULL);
    pthread_join(worker, NULL);

    signal(SIGINT, sigint);

    for(char temp;;) {
        int ret = read(fd, &temp, sizeof(temp));
        if(ret == -1) {
            fail("good, return immediately");
        } else if(ret == 0) {
            printf("Just return 0.\n");
        } else if(ret > 0) {
            printf("First round: %d.\n", ret);
        } else {
            // D status, never return.
        }
        sleep(1);
    }
    return 0;
}
