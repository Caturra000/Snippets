#include "meminfo_parser.hpp"
#include <sys/mman.h>


void parse(mp::Meminfo &meminfo) {
    if(mp::meminfo_parse(meminfo)) {
        throw std::runtime_error("error parse");
    }
}

int main() {
    constexpr static size_t _1KB = 1024;
    constexpr static size_t _1MB = 1024 * _1KB;
    constexpr static size_t _100MB = 100 * _1MB;
    constexpr size_t len = _100MB;

    mp::Meminfo m[4];
    parse(m[0]);

    void *addr = ::mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(addr == MAP_FAILED) {
        return 1;
    }

    parse(m[1]);

    mp::dump_diff(m[0], m[1], 100);
    ::puts("==========");

    #define ACCESS_ONCE(x) (*(volatile typeof(x)*)(&x))

    // test read
    size_t dummy;
    auto cur = reinterpret_cast<char*>(addr);
    for(size_t i = 0; i < len; ++i) {
        dummy += ACCESS_ONCE(cur[i]);
    }

    parse(m[2]);
    mp::dump_diff(m[1], m[2], 100);
    ::puts("==========");

    // test write
    for(size_t i = 0; i < len; ++i) {
        ACCESS_ONCE(cur[i]) = i;
    }

    parse(m[3]);
    mp::dump_diff(m[2], m[3], 100);

    ::munmap(addr, len);

    return 0;
}

// +Committed_AS:  102400 kB
// ==========
// -MemFree:       460 kB
// -MemAvailable:  444 kB
// +Active:        128 kB
// +Inactive:      116 kB
// +Inactive(anon):236 kB
// +Active(file):  128 kB
// -Inactive(file):120 kB
// +AnonPages:     372 kB
// ==========
// -MemFree:       101052 kB
// -MemAvailable:  101052 kB
// +Inactive:      102356 kB
// +Inactive(anon):102356 kB
// +AnonPages:     102324 kB


// 只测试写
// +Committed_AS:  102400 kB
// ==========
// ==========
// -MemFree:       104668 kB
// -MemAvailable:  104668 kB
// +Inactive:      102304 kB
// +Inactive(anon):102304 kB
// +AnonPages:     102556 kB
// +AnonHugePages: 100352 kB