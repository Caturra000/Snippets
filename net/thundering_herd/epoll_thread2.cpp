#include <sys/socket.h>
#include <sys/epoll.h>
#include "utils.h"

int main() {
    auto server_fd = make_server({.port=8848, .nonblock=false});

    auto work = [&] {
        int epfd = epoll_create1({}) | nofail("epoll_create");
        epoll_event watch_accept {.events = EPOLLIN, .data = {}};
        epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &watch_accept) | nofail("epoll_ctl");
        while(1) {
            epoll_event e;
            int n = epoll_wait(epfd, &e, 1, -1) | nofail("epoll_wait");
            if(n == 0) println("{} wakeup but ...?", gettid());
            else println("{} wakeup!", gettid()), accept(server_fd, {}, {});
            println("{} continued.", gettid());
        }
    };

    constexpr int thread_count = 5;
    auto threads = make_jthreads(thread_count, work);
    work();
}

// 每线程独占epoll实例，存在惊群
