#include <sys/time.h>
#include <bits/stdc++.h>

void testSyscall(size_t count) {
    for(; count--;) {
        // 立刻返回errno
        ::gettimeofday(nullptr, nullptr);
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

$ ./syscall_vdso 10000
total: 21500ns
average: 2.15ns

$ ./syscall_vdso 1000000
total: 215800ns
average: 2.158ns

$ ./syscall_vdso 100000000
total: 2.2049e+06ns
average: 2.2049ns

*/
