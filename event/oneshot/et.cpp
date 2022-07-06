#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <bits/stdc++.h>

static int pipefd[2];

void report(const std::string &reason, bool err, bool crash) {
    std::cerr << reason << " " << (err ? strerror(errno) : "") << std::endl;
    if(crash) abort();
}

constexpr auto interval = std::chrono::milliseconds(200);

void reader(int epfd) {
    while(1) {
        char buf[0xff];
        epoll_event event;
        int events = epoll_wait(epfd, &event, 1, -1);
        std::cout << "events:" << events << std::endl;
        if(events == 0) continue;
        // 假装不读
        // int n = read(pipefd[0], buf, sizeof buf);
        int n = 0;
        std::cerr << "read byte(s): " << n << std::endl;
        if(n < 0) {
            report("read failed:", true, false);
        }
        std::this_thread::sleep_for(interval * 10);
    }
}

void writer() {
    char ch = 'w';
    while(1) {
        write(pipefd[1], &ch, 1);
        std::this_thread::sleep_for(interval);
        // 如果去掉这里的注释，就是常见的ET坑
        // 如果再也没有event到来，且reader没有处理好上一次的event，那么就会陷入永远的阻塞
        // 但是如果有新的（同一个）event到来，即使reader没有处理好上一个event，仍能唤醒
        // break;
    }
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
        .events = EPOLLIN | EPOLLET
        // .events = EPOLLIN | EPOLLONESHOT
    };
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, pipefd[0], &e)) {
        report("epoll control failed:", true, true);
    }

    std::thread {writer}.detach();
    std::this_thread::sleep_for(interval * 10);
    std::thread {reader, epfd}.detach();


    int hang;
    std::cin >> hang;
    return 0;
}
