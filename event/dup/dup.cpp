#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <bits/stdc++.h>

static int pipefd[2];

void report(const std::string &reason, bool err, bool crash) {
    std::cerr << reason << " " << (err ? strerror(errno) : "") << std::endl;
    if(crash) abort();
}
int main() {

    if(pipe2(pipefd, O_NONBLOCK)) {
        report("pipe create failed:", true, true);
    }

    auto epfd = epoll_create(1);
    if(epfd < 0) {
        report("epoll create failed:", true, true);
    }
    epoll_event e {
        .events = EPOLLIN
    };

    epoll_event e2 {
        .events = EPOLLOUT
    };
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, pipefd[0], &e)) {
        report("epoll control failed:", true, true);
    }

    // 这里会触发report: File exists
    // 同一个fd确实不允许独立关注不同事件
    //
    // 那如果oneshot且同一个fd关注多个事件作为一个epoll_event，
    // 但是epoll_wait时只到来了部分事件，下一次会不会整个event都没了？
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, pipefd[0], &e2)) {
        report("epoll control failed:", true, true);
    }


    int hang;
    std::cin >> hang;
    return 0;
}
