#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <bits/stdc++.h>

static int pipefd[2];


int main() {

    signal(SIGINT, +[](int sig) {
        // assert sig == SIGINT
        char c = 'i';
        int n;
        if((n = write(pipefd[1], &c, 1)) < 0) {
            std::cerr << "write failed: "
                << strerror(errno) << std::endl;
            // abort();
        }
    });

    if(pipe2(pipefd, O_NONBLOCK)) {
        std::cerr << "pipe create failed: "
            << strerror(errno) << std::endl;
        abort();
    } 
    
    int serverPid;
    if((serverPid = fork()) == 0) {
        // 充当信号client
        close(pipefd[0]);
        while(1);
    } else if(serverPid > 0) {
        // 开始epoll测试，这里还需要继续fork
        close(pipefd[1]);
        int epfd = epoll_create(1);
        if(epfd < 0) {
            std::cerr << "epfd create failed: "
                << strerror(errno) << std::endl;
            abort();
        }
        epoll_event ee;
        ee.events = POLL_IN | EPOLLEXCLUSIVE;
        if(epoll_ctl(epfd, EPOLL_CTL_ADD, pipefd[0], &ee)) {
            std::cerr << "epoll control failed: "
                    << strerror(errno) << std::endl;
            abort();
        }
        if(fork() < 0) {
            std::cerr << "fork failed" << std::endl;
            abort();
        }
        // 都执行wait
        while(1) {
            constexpr static size_t EVENT_SIZE = PIPE_BUF;
            epoll_event revents[EVENT_SIZE];
            int ret = epoll_wait(epfd, revents, EVENT_SIZE, -1);
            std::cerr << "[wakeup!]" << ' ' << gettid() << std::endl;
            if(ret <= 0) {
                if(ret < 0 && errno != EAGAIN) {
                    std::cerr << "what? " << strerror(errno) << std::endl;
                }
                continue;
            }
            // 假定就只有一个事件
            if(revents[0].events & POLL_HUP) {
                std::cerr << "event hup" << std::endl;
                break;
            }
            if(revents[0].events & POLL_ERR) {
                std::cerr << "event error" << std::endl;
                break;
            }
            std::cerr << "ret: " << ret << std::endl;
            char buf[123];
            int hasRead = read(pipefd[0], buf, 123);
            std::cerr << "read: " << hasRead << std::endl;
        }
    }

    while(1);
    return 0;
}