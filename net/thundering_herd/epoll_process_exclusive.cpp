#include <sys/socket.h>
#include <sys/epoll.h>
#include "utils.h"

int main() {
    auto server_fd = make_server({.port=8848, .nonblock=false});

    int epfd = epoll_create1({}) | nofail("epoll_create");
    epoll_event watch_accept {.events = EPOLLIN | EPOLLEXCLUSIVE, .data = {}};
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &watch_accept) | nofail("epoll_ctl");

    constexpr int fork_count = 5;
    fork_and_forget(fork_count);

    while(1) {
        epoll_event e;
        int n = epoll_wait(epfd, &e, 1, -1) | nofail("epoll_wait");
        if(n == 0) println("{} wakeup but ...?", getpid());
        else println("{} wakeup!", getpid()), accept(server_fd, {}, {});
        println("{} continued.", getpid());
    }
}

// 多进程共享epoll实例，即使EPOLLEXCLUSIVE也存在惊群
