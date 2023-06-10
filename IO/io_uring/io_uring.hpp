#pragma once
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/io_uring.h>

/// Workaround for compatibility

int io_uring_setup(unsigned entries, struct io_uring_params *p) {
    return (int) syscall(__NR_io_uring_setup, entries, p);
}

int io_uring_enter(int ring_fd, unsigned int to_submit,
    unsigned int min_complete, unsigned int flags) {
    return (int) syscall(__NR_io_uring_enter, ring_fd, to_submit, min_complete,
                   flags, NULL, 0);
}
