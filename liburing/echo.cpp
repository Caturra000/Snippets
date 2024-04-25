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
    OP_TRAP,

    OP_ACCEPT,
    OP_READ,
    OP_WRITE,
    OP_CLOSE,

    OP_MAX
};

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

// This template makes `args...` passing the same as synchronous functions.
void async_operation(io_uring *uring, auto user_data, auto uring_prep_fn, auto &&...args) {
    auto sqe = io_uring_get_sqe(uring);
    // Equivalent to sync_fn(args...).
    uring_prep_fn(sqe, std::forward<decltype(args)>(args)...);
    io_uring_sqe_set_data(sqe, user_data);
    io_uring_submit(uring) | nofail<std::less_equal<int>>("io_uring_submit");
}

void async_accept(io_uring *uring, int server_fd) {
    async_operation(uring, data_pack(0, OP_ACCEPT),
        // Anon address.
        io_uring_prep_accept, server_fd, nullptr, nullptr, 0);
}

void async_read(io_uring *uring, int client_fd, auto &buf) {
    async_operation(uring, data_pack(client_fd, OP_READ),
        io_uring_prep_read, client_fd, buf.data(), std::size(buf), 0);
}

void async_write(io_uring *uring, int client_fd, const auto &buf, size_t size_bytes) {
    async_operation(uring, data_pack(client_fd, OP_WRITE),
        io_uring_prep_write, client_fd, buf.data(), size_bytes, 0);
}

void async_close(io_uring *uring, int client_fd) {
    async_operation(uring, data_pack(client_fd, OP_CLOSE),
        io_uring_prep_close, client_fd);
}

int main() {
    auto server_fd = make_server(8848);
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

    handlers[OP_TRAP] = [&](...) { std::terminate(); };

    // ACCEPT -> READ
    //        -> ACCEPT
    handlers[OP_ACCEPT] = [&](io_uring_cqe *cqe, auto) {
        auto client_fd = cqe->res | nofail("accept");
        auto &buf = buf_map[client_fd];
        async_read(&uring, client_fd, buf);
        // New customers are welcome.
        async_accept(&uring, server_fd);
    };

    // READ -> WRITE
    handlers[OP_READ] = [&](io_uring_cqe *cqe, uint32_t client_fd) {
        auto size_bytes = cqe->res | nofail("read");
        auto &buf = buf_map[client_fd];
        auto printer = std::ostream_iterator<char>{std::cout};
        // Print to stdout.
        std::copy_n(std::begin(buf), size_bytes, printer);
        async_write(&uring, client_fd, buf, size_bytes);
    };

    // WRITE -> CLOSE|READ
    handlers[OP_WRITE] = [&](io_uring_cqe *cqe, uint32_t client_fd) {
        auto size_bytes = cqe->res | nofail("write");
        auto &buf = buf_map[client_fd];
        // Zz-.
        bool close_proactive = size_bytes > 2 && buf[0] == 'Z' && buf[1] == 'z';
        // Zero-flag has written to client.
        bool close_reactive = (size_bytes == 0);
        if(close_reactive || close_proactive) {
            async_close(&uring, client_fd);
            buf_map.erase(client_fd);
            return;
        }
        async_read(&uring, client_fd, buf);
    };

    // Kick off!
    async_accept(&uring, server_fd);
    for(;;) {
        io_uring_cqe *cqe;
        io_uring_wait_cqe(&uring, &cqe) | nofail("io_uring_wait_cqe");
        auto cqe_cleanup = defer([&](...) { io_uring_cqe_seen(&uring, cqe); });

        auto [op, user_data] = data_unpack(cqe->user_data);
        if(auto &handler = handlers[op]; handler) {
            handler(cqe, user_data);
        }
    }
}
