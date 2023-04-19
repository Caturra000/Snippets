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

    void *addr = ::mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
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


// 如果测试读后再测试写
// +Committed_AS:  102400 kB
// ==========
// -MemFree:       101796 kB
// -MemAvailable:  101796 kB
// +Cached:        102236 kB
// +Inactive:      102540 kB
// +Inactive(anon):102540 kB
// +Mapped:        102236 kB
// +Shmem:         102236 kB
// ==========

// 如果把测试读过程注释掉，只测试写
// +Committed_AS:  102400 kB
// ==========
// ==========
// -MemFree:       102312 kB
// -MemAvailable:  102312 kB
// +Cached:        102236 kB
// +Inactive:      102480 kB
// +Inactive(anon):102480 kB
// +Mapped:        102236 kB
// +Shmem:         102236 kB