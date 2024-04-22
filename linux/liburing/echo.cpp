#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <liburing.h>
#include <format>
#include <iostream>
#include <ranges>
#include <string_view>
#include <vector>
#include <memory>
#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <functional>
#include "utils.h"

enum OP: uint32_t {
    OP_ACCEPT = 1,
    OP_READ,
    OP_WRITE,

    OP_MAX
};

int make_listen(int port) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    check(socket_fd < 0, "socket");
    int enable = 1;
    int opt_ret = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    check(opt_ret < 0, "setsockopt");

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int bind_ret = bind(socket_fd, std::bit_cast<const sockaddr *>(&addr), sizeof(addr));
    check(bind_ret < 0, "bind");

    int listen_ret = listen(socket_fd, 128);
    check(listen_ret, "listen");

    return socket_fd;
}

// For cqe->user_data.
void* data_pack(uint32_t high32, uint32_t low32) {
    return reinterpret_cast<void*>(
        static_cast<uint64_t>(high32) << 32 | low32);
}

auto data_unpack(auto data) {
    auto data64 = std::bit_cast<uint64_t>(data);
    auto low32 = static_cast<uint32_t>(data64);
    auto high32 = data64 >> 32;
    return std::tuple(low32, high32);
}

void async_accept(io_uring *uring, int server_fd) {
    auto sqe = io_uring_get_sqe(uring);
    // Anon address.
    io_uring_prep_accept(sqe, server_fd, nullptr, nullptr, 0);
    io_uring_sqe_set_data(sqe, data_pack(0, OP_ACCEPT));
    io_uring_submit(uring);
}

void async_read(io_uring *uring, int client_fd, auto &buf) {
    auto sqe = io_uring_get_sqe(uring);
    io_uring_prep_read(sqe, client_fd, buf.data(), std::size(buf), 0);
    io_uring_sqe_set_data(sqe, data_pack(client_fd, OP_READ));
    io_uring_submit(uring);
}

void async_write(io_uring *uring, int client_fd, const auto &buf, size_t size_bytes) {
    auto sqe = io_uring_get_sqe(uring);
    io_uring_prep_write(sqe, client_fd, buf.data(), size_bytes, 0);
    io_uring_sqe_set_data(sqe, data_pack(client_fd, OP_WRITE));
    io_uring_submit(uring);
}

int main() {
    auto server_fd = make_listen(8848);
    auto server_fd_cleanup = defer([&](...) { close(server_fd); });

    io_uring uring;
    constexpr size_t ENTRIES = 256;
    io_uring_queue_init(ENTRIES, &uring, 0);
    auto uring_cleanup = defer([&](...) { io_uring_queue_exit(&uring); });

    // {fd: buf}
    using Buffer_type = std::array<char, 4096>;
    std::unordered_map<int, Buffer_type> buf_map;

    // Format: |COMPLETE| -> |ISSUE|.
    using Handler = std::function<void(io_uring_cqe*, uint32_t)>;
    std::array<Handler, OP_MAX> handlers;

    handlers[0] = [&](...) { exit(998244353); };

    // ACCEPT -> READ
    //        -> ACCEPT
    handlers[OP_ACCEPT] = [&](io_uring_cqe *cqe, auto) {
        auto client_fd = cqe->res;
        check(client_fd < 0, "accept");
        auto &buf = buf_map[client_fd];
        async_read(&uring, client_fd, buf);
        // New customers are welcome.
        async_accept(&uring, server_fd);
    };

    // READ -> WRITE
    handlers[OP_READ] = [&](io_uring_cqe *cqe, uint32_t client_fd) {
        auto size_bytes = cqe->res;
        check(size_bytes < 0, "read");
        auto &buf = buf_map[client_fd];
        auto printer = std::ostream_iterator<char>{std::cout};
        // Print to stdout.
        std::copy_n(std::begin(buf), size_bytes, printer);
        async_write(&uring, client_fd, buf, size_bytes);
    };

    // WRITE -> CLOSE|READ
    handlers[OP_WRITE] = [&](io_uring_cqe *cqe, uint32_t client_fd) {
        auto size_bytes = cqe->res;
        check(size_bytes < 0, "write");
        auto &buf = buf_map[client_fd];
        // Close check. (Zz-)
        if(size_bytes > 2 && buf[0] == 'Z' && buf[1] == 'z') {
            close(client_fd);
            buf_map.erase(client_fd);
        // Echo again.
        } else {
            async_read(&uring, client_fd, buf);
        }
    };

    // Kick off!
    async_accept(&uring, server_fd);
    for(;;) {
        io_uring_cqe *cqe;
        auto wait_ret = io_uring_wait_cqe(&uring, &cqe);
        check(wait_ret < 0, "io_uring_wait_cqe");
        check(cqe->res < 0, "event");
        auto cqe_cleanup = defer([&](...) { io_uring_cqe_seen(&uring, cqe); });

        auto [op, user_data] = data_unpack(cqe->user_data);

        handlers[op](cqe, user_data);
    }
}
