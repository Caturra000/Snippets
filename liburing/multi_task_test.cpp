#include <fcntl.h>
#include <string>
#include <ranges>
#include <algorithm>
#include "utils.h"
#include "coroutine.h"

Task mywrite(auto uring, int fd, char c) {
    co_await async_write(uring, fd, &c, 1);
}

Task mywrite_v2(auto uring, int fd, char c) {
    auto buf = std::string("\e[31m") + c + "\e[0m";
    co_await async_write(uring, fd, buf.data(), buf.size());
}

Task multitask(auto uring, auto &io_context, int fd) {
    for(;;) for(auto c : std::views::iota('A', 'Z')) {
        // Random order.
        co_spawn(io_context, mywrite_v2(uring, fd, c));

        // Strict order.
        // co_await mywrite_v2(uring, fd, c);

        co_await mywrite(uring, fd, c - 'A' + 'a');
    }
}

int main() {
    io_uring uring;
    constexpr size_t ENTRIES = 256;
    io_uring_queue_init(ENTRIES, &uring, 0);
    auto uring_cleanup = defer([&](...) { io_uring_queue_exit(&uring); });

    Io_context io_context{uring};
    co_spawn(io_context, multitask(&uring, io_context, 1 /*stdout*/));
    io_context.run();
}
