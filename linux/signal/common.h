#pragma once
#include <unistd.h>
#include <signal.h>
#include <iostream>
#include <cassert>
#include <cstring>
#include <string_view>

void output(std::string_view info) {
    std::cout << info << ":\t"
              << gettid() << ", "
              << getpid() << ", "
              << getppid() << ", "
              << getpgid(0) << ", "
              << getsid(0) << std::endl;
}

// May be interrupted.
void hang() {
    char c;
    read(0, &c, 1);
}

void hang_strong() {
    char c;
    while(read(0, &c, 1) == EINTR) continue;
}

// 这是一种对信号很糟糕的写法，不过这里只是一些小示例
void signal_handler(int signum) {
    std::cout << getpid() << ": " << strsignal(signum) << std::endl;
}

void register_signal_handler() {
    // 标准信号，不包括实时信号
    for(int i = 1; i <= 31; ++i) {
        struct sigaction sa {};
        sa.sa_handler = signal_handler;
        sigaction(i, &sa, nullptr);
    }
}