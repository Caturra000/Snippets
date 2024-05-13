#include <sys/socket.h>
#include <sys/epoll.h>
#include "utils.h"

int main() {
    auto master_fd = make_server({.port=8848});
    auto slave_fd = make_server({.port=8849});

    int epfd = epoll_create1({}) | nofail("epoll_create");

    constexpr int fork_count = 1;
    bool main_process = fork_and_forget(fork_count);

    epoll_event watch_accept {.events = EPOLLIN, .data = {}};
    int server_fd = main_process ? master_fd : slave_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &watch_accept) | nofail("epoll_ctl");

    while(1) {
        epoll_event e;
        int n = epoll_wait(epfd, &e, 1, -1) | nofail("epoll_wait");
        if(n == 0) println("{} wakeup but ...?", getpid());
        else println("{} wakeup!", getpid()), accept(server_fd, {}, {});
        println("{} continued.", getpid());
    }
}

// 多进程共享epoll实例，主从关注事件独立，存在惊群
// NOTE: 由于不允许重复添加事件（8849可读），这里只能fork出1个进程
