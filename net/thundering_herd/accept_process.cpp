#include <sys/socket.h>
#include "utils.h"

int main() {
    auto server_fd = make_server({.port=8848});

    constexpr int fork_count = 5;
    bool main_process = fork_and_forget(fork_count);

    println("Server {} is ready. {}", getpid(), main_process ? "[main]" : "[child]");

    while(1) {
        accept(server_fd, {}, {}) | nofail("accept");
        println("[{}] wakeup!", getpid());
    }
}

// accept已不存在惊群
