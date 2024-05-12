#pragma once
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <type_traits>
#include <format>
#include <iostream>
#include <thread>
#include <vector>
#include <ranges>
#include <syncstream>

// C++-style check for syscall.
// Failed on ret < 0 by default.
//
// INT_ASYNC_CHECK: helper for liburing (-ERRNO) and other syscalls (-1).
// It may break generic programming (forced to int).
template <typename Comp = std::less<int>, auto V = 0, bool INT_ASYNC_CHECK = true>
struct nofail {
    std::string_view reason;

    // Examples:
    // fstat(...) | nofail("fstat");        // Forget the if-statement and ret!
    // int fd = open(...) | nofail("open"); // If actually need a ret, here you are!
    friend decltype(auto) operator|(auto &&ret, nofail nf) {
        if(Comp{}(ret, V)) [[unlikely]] {
            // Hack errno.
            if constexpr (INT_ASYNC_CHECK) {
                using T = std::decay_t<decltype(ret)>;
                static_assert(std::is_convertible_v<T, int>);
                // -ERRNO
                if(ret != -1) errno = -ret;
            }
            perror(nf.reason.data());
            std::terminate();
        }
        return std::forward<decltype(ret)>(ret);
    };
};

// Make clang happy.
nofail(...) -> nofail<std::less<int>, 0, true>;

// Go-style, move-safe defer.
[[nodiscard("defer() is not allowed to be temporary.")]]
inline auto defer(auto func) {
    // Make STL happy.
    auto dummy = reinterpret_cast<void*>(0x1);
    return std::unique_ptr<void, decltype(func)>{dummy, std::move(func)};
}

// For make_server().
struct make_server_option_t {
    int port {8848};
    bool nonblock {false};
    bool reuseport {false};
    int backlog {128};
};

// Do some boring stuff and return a server fd.
inline int make_server(make_server_option_t option = {}) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, option.nonblock ? SOCK_NONBLOCK : 0) | nofail("socket");
    int enable = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) | nofail("setsockopt");
    if(option.reuseport) {
        int enable = 1;
        setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) | nofail("setsockopt");
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(option.port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(socket_fd, std::bit_cast<const sockaddr *>(&addr), sizeof(addr)) | nofail("bind");

    listen(socket_fd, option.backlog) | nofail("listen");

    return socket_fd;
}

template<typename ...Ts>
inline void println(std::format_string<Ts...> fmt, Ts &&...args) {
    auto str = std::format(fmt, std::forward<Ts>(args)...);
    std::osyncstream(std::cout) << str << std::endl;
}

// Return is_main.
inline bool fork_and_forget(size_t n_children) {
    while(n_children--) {
        auto any_pid = fork() | nofail("fork");
        if(!any_pid) {
            return false;
        }
    }
    return true;
}

inline auto make_jthreads(size_t n_threads, auto &&work, auto &&...args) {
    std::vector<std::jthread> threads;
    while(n_threads--) {
        threads.emplace_back(work, args...);
    }
    return threads;
}
