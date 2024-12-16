#include "common.h"

int main() {
    int f = fork();
    if(f < 0) {
        std::cerr << strerror(errno) << std::endl;
        return 1;
    }
    // 父死，子成为非PG leader的进程
    if(f > 0) {
        raise(SIGINT);
        while(1);
    }
    // setsid()  creates  a new session **if** the calling process is NOT a process group leader.
    auto sid = setsid();
    if(sid < 0) {
        std::cerr << strerror(errno) << std::endl;
        return 3;
    }
    register_signal_handler();
    std::cout << "setsid returns: " << sid << std::endl;
    output("main");
    auto pid = fork();
    assert(pid >= 0);
    if(!pid) {
        output("child");
        hang_strong();
        // 按理说，应该有SIGHUP才对……
        output("child(2)");
        // 但它还是活着
        while(1);
    } else {
        raise(SIGINT);
        return 0;
    }
}
