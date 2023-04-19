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
// -MemFree:       145884 kB
// -MemAvailable:  10336 kB
// +Cached:        135668 kB
// +Active:        1224 kB
// +Inactive:      145072 kB
// +Inactive(anon):10856 kB
// +Active(file):  1220 kB
// +Inactive(file):134216 kB
// +Dirty: 1708 kB
// +AnonPages:     10844 kB
// +Mapped:        120784 kB
// +KReclaimable:  224 kB
// +Slab:  224 kB
// +SReclaimable:  224 kB
// +PageTables:    240 kB
// +Committed_AS:  99076 kB
// +AnonHugePages: 2048 kB
// ==========
// -MemFree:       39044 kB
// -MemAvailable:  8532 kB
// +Cached:        29296 kB
// +Active:        564 kB
// +Inactive:      35944 kB
// +Inactive(anon):7220 kB
// +Active(file):  560 kB
// +Inactive(file):28724 kB
// +Dirty:         103944 kB
// +AnonPages:     7260 kB
// +Mapped:        26320 kB
// +KReclaimable:  2460 kB
// +Slab:  2496 kB
// +SReclaimable:  2460 kB
// +PageTables:    136 kB
// +Committed_AS:  768 kB


// 如果不测试读，只测试写
// ==========
// ==========
// -MemFree:       104792 kB
// -MemAvailable:  1008 kB
// +Cached:        102400 kB
// +Inactive:      102584 kB
// +Inactive(anon):192 kB
// +Inactive(file):102392 kB
// +Dirty:         102388 kB
// +AnonPages:     160 kB
// +Mapped:        102328 kB
// +KReclaimable:  2784 kB
// +Slab:  2840 kB
// +SReclaimable:  2784 kB
// +PageTables:    208 kB
