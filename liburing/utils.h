#pragma once
#include <cstdlib>
#include <memory>
#include <string_view>

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

    // Example:
    // fstat(...) | nofail("fstat");        // Forget the if-statement and ret!
    // int fd = open(...) | nofail("open"); // If actually need a ret, here you are!
    friend decltype(auto) operator|(auto &&ret, nofail nf) {
        check(Comp{}(ret, V), nf.reason);
        return std::forward<decltype(ret)>(ret);
    };
};

// Go-style, movable defer.
[[nodiscard("defer() is not allowed to be temporary.")]]
inline auto defer(auto func) {
    // Make STL happy.
    auto dummy = reinterpret_cast<void*>(0x1);
    return std::unique_ptr<void, decltype(func)>{dummy, std::move(func)};
}
