#include "common.h"

int main() {
    output("main");
    auto pid = fork();
    assert(pid >= 0);
    if(!pid) {
        output("child");
        // 父进程收到信号，子进程不会退出
        // 即使它本身是PG leader也不影响
        hang_strong();
        output("child(2)");
    } else {
        raise(SIGINT);
    }
}
