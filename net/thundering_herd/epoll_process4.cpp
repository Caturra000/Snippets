#include <sys/socket.h>
#include <sys/epoll.h>
#include "utils.h"

int main() {
    auto master_fd = make_server({.port=8848, .nonblock=false});
    auto slave_fd = make_server({.port=8849, .nonblock=false});

    constexpr int fork_count = 5;
    bool main_process = fork_and_forget(fork_count);

    int epfd = epoll_create1({}) | nofail("epoll_create");
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

// 每进程独占epoll实例，主从关注事件独立，惊群独立
// NOTE: 等待8848和等待8849的进程群的惊群现象互不干扰
