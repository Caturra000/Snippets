#include <unistd.h>
#include <netinet/in.h>
#include <liburing.h>
#include <utility>
#include <iostream>
#include <algorithm>
#include <array>
#include <ranges>
#include <iterator>
#include "utils.h"
#include "coroutine.h"

Task echo(io_uring *uring, int client_fd) {
    char buf[4096];
    for(;;) {
        auto n = co_await async_read(uring, client_fd, buf, std::size(buf)) | nofail("read");

        auto printer = std::ostream_iterator<char>{std::cout};
        std::ranges::copy_n(buf, n, printer);

        n = co_await async_write(uring, client_fd, buf, n) | nofail("write");

        bool close_proactive = n > 2 && buf[0] == 'Z' && buf[1] == 'z';
        bool close_reactive = (n == 0);
        if(close_reactive || close_proactive) {
            co_await async_close(uring, client_fd);
            break;
        }
    }
}

Task server(io_uring *uring, Io_context &io_context, int server_fd) {
    for(;;) {
        auto client_fd = co_await async_accept(uring, server_fd) | nofail("accept");
        // Fork a new connection.
        co_spawn(io_context, echo(uring, client_fd));
    }
}

int main() {
    auto server_fd = make_server(8848);
    auto server_fd_cleanup = defer([&](...) { close(server_fd); });

    io_uring uring;
    constexpr size_t ENTRIES = 256;
    io_uring_queue_init(ENTRIES, &uring, 0);
    auto uring_cleanup = defer([&](...) { io_uring_queue_exit(&uring); });

    Io_context io_context{uring};
    co_spawn(io_context, server(&uring, io_context, server_fd));
    io_context.run();
}
