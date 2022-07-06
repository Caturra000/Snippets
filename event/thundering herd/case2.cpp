#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <bits/stdc++.h>


// case2与case1唯一不同就在于server()函数中，epoll实例是fork后才每个task分别创建的
// 在linux内核大于等于4.5的版本可以看出，这种做法才能有效抑制惊群效应
// （比如我的老爷机WSL1刚好在4.4版本，于是看不到效果）
//
// 如果再进一步，把事件的EPOLLEXCLUSIVE去掉
// 那么惊群问题又会复现


static int pipefd[2];

void report(const std::string &reason, bool err, bool crash) {
    std::cerr << reason << " " << (err ? strerror(errno) : "") << std::endl;
    if(crash) abort();
}

void signalClient() {
    // 手动输入
    // for(char c; std::cin >> c;) {
    //     int n;
    //     if((n = write(pipefd[1], &c, 1)) < 0) {
    //         report("write failed:", true, false);
    //     } else if(n == 0) {
    //         report("write n==0", false, false);
    //     }
    // }

    // 自动定时写入
    for(;;) {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(2s);
        char c = '1';
        int n;
        if((n = write(pipefd[1], &c, 1)) < 0) {
            report("write failed:", true, false);
        } else if(n == 0) {
            report("write n==0", false, false);
        }
    }
}

void server(int serverPid) {
    // 开始epoll测试，这里还需要继续fork
    report("this server tid:" + std::to_string(gettid()), false, false);
    // 再fork一些方便参考
    for(auto _{2}; _--;) if(fork() < 0) {
        report("fork failed:", false, true);
    }
    int epfd = epoll_create(1);
    if(epfd < 0) {
        report("epfd create failed:", true, true);
    }
    epoll_event ee;
    ee.events = POLL_IN | EPOLLEXCLUSIVE;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, pipefd[0], &ee)) {
        report("epoll control failed:", true, true);
    }
    // 都执行wait
    while(1) {
        constexpr static size_t EVENT_SIZE = PIPE_BUF;
        epoll_event revents[EVENT_SIZE];
        int ret = epoll_wait(epfd, revents, EVENT_SIZE, -1);
        // atomic output
        std::stringstream ss;
        // 任意退出条件都不会错过输出
        std::shared_ptr<void> guard {nullptr, [&](void*) {
            ss << '\n';
            std::cerr << ss.str();
        }};
        ss << "[wakeup!]" << ' ' << gettid() << std::endl;
        if(ret <= 0) {
            if(ret < 0 && errno != EAGAIN) {
                ss << "what? " << strerror(errno) << std::endl;
            }
            continue;
        }
        // 假定就只有一个事件
        if(revents[0].events & POLL_HUP) {
            ss << "event hup" << std::endl;
            break;
        }
        if(revents[0].events & POLL_ERR) {
            ss << "event error" << std::endl;
            break;
        }
        ss << "ret: " << ret << std::endl;
        char buf[123];
        int hasRead = read(pipefd[0], buf, 123);
        ss << "read: " << hasRead << std::endl;
    }
}

int main() {

    if(pipe2(pipefd, O_NONBLOCK)) {
        report("pipe create failed:", true, true);
    }

    int serverPid;
    if((serverPid = fork()) == 0) {
        // 充当信号client
        close(pipefd[0]);
        signalClient();
    } else if(serverPid > 0) {
        close(pipefd[1]);
        server(serverPid);
    }

    while(1);
    return 0;
}