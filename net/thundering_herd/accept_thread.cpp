#include <sys/socket.h>
#include "utils.h"

int main() {
    auto server_fd = make_server({.port=8848, .nonblock=false});

    constexpr int thread_count = 5;

    auto work = [&](bool main_thread) {
        println("Thread {} is ready. {}", gettid(), main_thread ? "[main]" : "[child]");
        while(1) {
            accept(server_fd, {}, {}) | nofail("accept");
            println("[{}] wakeup!", gettid());
        }
    };

    auto threads = make_jthreads(thread_count - 1, work, false);
    work(true);
}

// accept已不存在惊群
