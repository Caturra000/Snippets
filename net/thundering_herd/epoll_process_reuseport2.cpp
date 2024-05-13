#include <sys/socket.h>
#include <sys/epoll.h>
#include "utils.h"

int main() {
    constexpr int fork_count = 5;
    fork_and_forget(fork_count);

    auto server_fd = make_server({.port=8848, .nonblock=false, .reuseport=true});

    int epfd = epoll_create1({}) | nofail("epoll_create");
    epoll_event watch_accept {.events = EPOLLIN, .data = {}};
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &watch_accept) | nofail("epoll_ctl");

    while(1) {
        epoll_event e;
        int n = epoll_wait(epfd, &e, 1, -1) | nofail("epoll_wait");
        if(n == 0) println("{} wakeup but ...?", getpid());
        else println("{} wakeup!", getpid()), accept(server_fd, {}, {});
        println("{} continued.", getpid());
    }
}

// 每进程独占epoll实例，使用REUSEPORT负载均衡不存在惊群
