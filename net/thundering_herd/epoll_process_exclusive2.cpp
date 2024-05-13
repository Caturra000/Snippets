#include <sys/socket.h>
#include <sys/epoll.h>
#include "utils.h"

int main() {
    auto server_fd = make_server({.port=8848});

    constexpr int fork_count = 5;
    fork_and_forget(fork_count);

    int epfd = epoll_create1({}) | nofail("epoll_create");
    epoll_event watch_accept {.events = EPOLLIN | EPOLLEXCLUSIVE, .data = {}};
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &watch_accept) | nofail("epoll_ctl");

    while(1) {
        epoll_event e;
        int n = epoll_wait(epfd, &e, 1, -1) | nofail("epoll_wait");
        if(n == 0) println("{} wakeup but ...?", getpid());
        else println("{} wakeup!", getpid()), accept(server_fd, {}, {});
        println("{} continued.", getpid());
    }
}

// 每进程独占epoll实例，使用EPOLLEXCLUSIVE不存在惊群
// 可对比：
// 共享epoll，使用EPOLLEXCLUSIVE：epoll_process_exclusive
// 独占epoll，不使用EPOLLEXCLUSIVE：epoll_process2
