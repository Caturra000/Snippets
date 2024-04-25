#pragma once
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <bit>

// C-style check for syscall.
inline void check(int cond, std::string_view reason) {
    if(!cond) [[likely]] return;
    perror(reason.data());
    abort();
}

// C++-style check for syscall.
// Failed on ret < 0 by default.
template <typename Comp = std::less<int>, auto V = 0>
struct nofail {
    std::string_view reason;

    // Examples:
    // fstat(...) | nofail("fstat");        // Forget the if-statement and ret!
    // int fd = open(...) | nofail("open"); // If actually need a ret, here you are!
    friend decltype(auto) operator|(auto &&ret, nofail nf) {
        check(Comp{}(ret, V), nf.reason);
        return std::forward<decltype(ret)>(ret);
    };
};

// Make clang happy.
nofail(...) -> nofail<std::less<int>, 0>;

// Go-style, movable defer.
[[nodiscard("defer() is not allowed to be temporary.")]]
inline auto defer(auto func) {
    // Make STL happy.
    auto dummy = reinterpret_cast<void*>(0x1);
    return std::unique_ptr<void, decltype(func)>{dummy, std::move(func)};
}

// Do some boring stuff and return a server fd.
int make_server(int port) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0) | nofail("socket");
    int enable = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) | nofail("setsockopt");

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(socket_fd, std::bit_cast<const sockaddr *>(&addr), sizeof(addr)) | nofail("bind");

    listen(socket_fd, 128) | nofail("listen");

    return socket_fd;
}
