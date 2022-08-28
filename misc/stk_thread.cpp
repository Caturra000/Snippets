#include <bits/stdc++.h>

int main() {
    std::function<void()> recursive = [&] {
        static size_t depth = 1;
        char buf[1<<20];
        std::cout << depth++ << std::endl;
        recursive();
    };
    std::thread t {recursive};
    t.join();
    return 0;
}

// 输出测试：
// 1
// 2
// 3
// 4
// 5
// 6
// 7
// 终于爆栈
