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

    // 预先准备一个大小100MB的test_mmap.bin
    int fd = ::open("test_mmap.bin", O_RDWR);
    if(fd < 0) {
        return 1;
    }

    mp::Meminfo m[4];
    parse(m[0]);

    void *addr = ::mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
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
// -MemFree:       102552 kB
// -MemAvailable:  148 kB
// +Cached:        102400 kB
// +Inactive:      102556 kB
// +Inactive(anon):168 kB
// +Inactive(file):102388 kB
// +AnonPages:     172 kB
// +Mapped:        102368 kB
// +PageTables:    204 kB
// ==========
// -MemFree:       102564 kB
// -MemAvailable:  102564 kB
// +Inactive:      102236 kB
// +Inactive(anon):102236 kB
// +AnonPages:     102236 kB
// -Mapped:        102236 kB


// 如果不测试读，只测试写
// +Committed_AS:  102400 kB
// ==========
// ==========
// -MemFree:       203856 kB
// -MemAvailable:  101392 kB
// +Cached:        102400 kB
// +Inactive:      204800 kB
// +Inactive(anon):102400 kB
// +Inactive(file):102400 kB
// +AnonPages:     102444 kB
// +KReclaimable:  128 kB
// +Slab:  128 kB
// +SReclaimable:  128 kB
// +PageTables:    204 kB