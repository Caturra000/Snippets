#include <unistd.h>
#include <bits/stdc++.h>

void testSyscall(size_t count) {
    for(; count--;) {
        // 立刻返回errno
        ::read(-1, nullptr, 0);
    }
}

int main(int argc, const char *argv[]) {
    if(argc <= 1) {
        std::cerr << "[ERR] no input" << std::endl;
        return -1;
    }
    const size_t count = ::atoi(argv[1]);
    auto start = std::chrono::system_clock::now();
    testSyscall(count);
    auto end = std::chrono::system_clock::now();

    const auto delta = end - start;
    const std::chrono::duration<double, std::nano> elasped {delta};
    std::cout << "total: " << elasped.count() << "ns" << std::endl;
    std::cout << "average: " << elasped.count() / count << "ns" << std::endl;
    return 0;
}

// 4750U简单测试
/*

$ ./syscall 10000
total: 492700ns
average: 49.27ns

$ ./syscall 1000000
total: 4.89064e+07ns
average: 48.9064ns

$ ./syscall 100000000
total: 4.95154e+09ns
average: 49.5154ns

*/
