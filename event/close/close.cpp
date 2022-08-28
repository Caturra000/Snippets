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


    if(epoll_ctl(epfd, EPOLL_CTL_ADD, pipefd[0], &e)) {
        report("epoll control failed:", true, true);
    }

    write(pipefd[1], "x", 1);


    // CASE1:
    // 不进行EPOLL_CTL_DEL，但是直接关掉pipefd
    // 测试得出epoll内部并不会持有refcount
    // 已经无法得出唯一的epoll wait结果
    // close(pipefd[0]);

    // CASE2:
    // 同样是close，但是这里dup对应的fd，以提供refcount
    // 此时仍可以得出epoll wait结果（虽然我觉得没啥实用价值）
    // int shadow = dup2(pipefd[0], 999);
    // if(shadow != 999) {
    //     report("dup failed:", true, true);
    // }
    // close(pipefd[0]);


    epoll_event events[3] {};

    int count = epoll_wait(epfd, events, 3, -1);

    if(count >= 0) {
        std::cout << "count: " << count << std::endl;
    } else {
        report("epoll wait failed:", true, true);
    }

    return 0;
}