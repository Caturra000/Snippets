#include <unistd.h>
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    std::cout << ::getpid() << std::endl;
    while(1) {
        using namespace std::chrono_literals;
        auto &now = std::chrono::steady_clock::now;
        auto last = now();
        while(now() - last < 10s) /* busy loop */;
        while(now() - last < 20s) std::this_thread::sleep_for(1ms);
    }
}
