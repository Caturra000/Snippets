#pragma once
#include <cstdlib>
#include <memory>
#include <string_view>

void check(int cond, std::string_view reason) {
    if(!cond) return;
    perror(reason.data());
    abort();
}

auto defer(auto func) {
    // Make STL happy.
    auto dummy = reinterpret_cast<void*>(0x1);
    return std::unique_ptr<void, decltype(func)>{dummy, std::move(func)};
}
