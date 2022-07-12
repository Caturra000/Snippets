#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <bits/stdc++.h>


// 构造一个oneshot可能无条件移除的例子
// 对于server和client之间accept得到的connection
// 1. 一开始是可写状态（client未write）
// 2. 然后进入可读可写状态
//
// 这里epoll会关注读、写，并打上oneshot标记
// 看下步骤2能不能复现
//
// 测试结果是，ONESHOT真的会无条件干掉，即使有部分event未到达

void report(const std::string &reason, bool err, bool crash) {
    std::cerr << reason << " " << (err ? strerror(errno) : "") << std::endl;
    if(crash) abort();
}
int main() {

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(2345);

    if(serverSocket < 0) {
        report("socket failed: ", true, true);
    }
    if(clientSocket < 0) {
        report("socket failed: ", true, true);
    }

    int opt = 1;
    if(setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, (socklen_t)(sizeof opt))) {
        report("setsockopt failed:", true, true);
    }

    if(bind(serverSocket, (const sockaddr*)&addr, sizeof(addr))) {
        report("bind failed:", true, true);
    }

    if(listen(serverSocket, 1)) {
        report("listen failed", true, true);
    }


    std::thread {[=] {
        if(connect(clientSocket, (const sockaddr*)&addr, sizeof(addr))) {
            report("connect failed:", true, false);
        }

        std::cout << "connected." << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(3));

        char x = 'x';
        int n = write(clientSocket, &x, 1);
        if(n < 0) {
            report("write failed:", true, false);
        } else if(n == 0) {
            report("n == 0", false, false);
        } else {
            std::cout << "write 1 byte." << std::endl;
        }

        while(1);
    }}.detach();


    sockaddr_in peer {};
    socklen_t len;
    int connection = accept(serverSocket, (sockaddr*)&peer, &len);

    auto epfd = epoll_create(1);
    if(epfd < 0) {
        report("epoll create failed:", true, true);
    }
    epoll_event e {
        .events = EPOLLIN | EPOLLOUT | EPOLLONESHOT
        // .events = EPOLLIN | EPOLLOUT
    };

    if(epoll_ctl(epfd, EPOLL_CTL_ADD, connection, &e)) {
        report("epoll control failed:", true, true);
    }

    while(1) {
        epoll_event e = {};
        int n = epoll_wait(epfd, &e, 1, 100);
        std::cout << "events: " << n << std::endl;
        if(e.events & EPOLLIN) {
            std::cout << "IN" << std::endl;
        }
        if(e.events & EPOLLOUT) {
            std::cout << "OUT" << std::endl;
        }
        if(e.events & EPOLLRDHUP) {
            std::cout << "RDHUP" << std::endl;
        }
        if(e.events & EPOLLERR) {
            std::cout << "ERR" << std::endl;
        }
        std::cout << "============" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }


    int hang;
    std::cin >> hang;
    return 0;
}
