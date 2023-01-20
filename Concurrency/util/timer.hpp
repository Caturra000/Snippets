#pragma once
#include <bits/stdc++.h>

struct {
    std::chrono::steady_clock::time_point clock_start, clock_end;
} global;

[[gnu::constructor]]
void global_start() {
    global.clock_start = std::chrono::steady_clock::now();
}

[[gnu::destructor]]
void global_end() {
    global.clock_end = std::chrono::steady_clock::now();
    using ToMilli = std::chrono::duration<double, std::milli>;
    auto elapsed = ToMilli{global.clock_end - global.clock_start}.count();
    std::cout << "elapsed: " << elapsed << "ms" << std::endl;
}
