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

    void *addr = ::mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
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

// ==========
// -MemFree:       106332 kB
// -MemAvailable:  264 kB
// +Cached:        105132 kB
// +Active:        3404 kB
// +Inactive:      105236 kB
// +Active(anon):  2772 kB
// +Active(file):  632 kB
// +Inactive(file):105236 kB
// +Dirty: 2024 kB
// +AnonPages:     3020 kB
// +Mapped:        102324 kB
// +KReclaimable:  396 kB
// +Slab:  580 kB
// +SReclaimable:  396 kB
// +SUnreclaim:    184 kB
// +PageTables:    652 kB
// +AnonHugePages: 6144 kB
// ==========
// -MemFree:       5544 kB
// -MemAvailable:  3176 kB
// +Cached:        1252 kB
// +Active:        1896 kB
// +Inactive:      1056 kB
// +Active(anon):  1888 kB
// +Inactive(file):1056 kB
// +Dirty: 103064 kB
// +AnonPages:     1900 kB
// +KReclaimable:  2612 kB
// +Slab:  2612 kB
// +SReclaimable:  2612 kB


// 如果不测试读，只测试写
// ==========
// ==========
// -MemFree:       110460 kB
// -MemAvailable:  3880 kB
// +Cached:        104288 kB
// +Active:        4044 kB
// +Inactive:      104276 kB
// +Active(anon):  3216 kB
// +Active(file):  828 kB
// +Inactive(file):104276 kB
// +Dirty: 103796 kB
// +AnonPages:     3016 kB
// +Mapped:        102512 kB
// +KReclaimable:  2956 kB
// +Slab:  3124 kB
// +SReclaimable:  2956 kB
// +SUnreclaim:    168 kB
// +PageTables:    264 kB
// +AnonHugePages: 6144 kB
