// godbolt online test
// sorted: elasped: 429.784ms
// random: elasped: 1038.77ms


#include <algorithm>
#include <iostream>
#include <chrono>
#include <array>

constexpr int MAXN = 10000;
constexpr int MAXV = 256;
std::array<int, MAXN> arr;

int main(int argc, const char *argv[]) {

    for(auto &v : arr) v = ::rand() % MAXV;

    if(argc >= 2 && argv[1][0] == 's') {
        std::cout << "sorted" << std::endl;
        std::sort(arr.begin(), arr.end());
    } else {
        std::cout << "random" << std::endl;
    }

    int sum = 0;

    auto start = std::chrono::system_clock::now();

    for(int count = 100000; count--;) {
        sum += std::count_if(arr.begin(), arr.end(), [](int v) {
            return v >= MAXV / 2;
        });
    }

    auto end = std::chrono::system_clock::now();

    using ToMilli = std::chrono::duration<double, std::milli>;

    std::cout << "sum: " << sum << std::endl;
    std::cout << "elasped: " << ToMilli{end - start}.count() << "ms" << std::endl;
    return 0;
}
