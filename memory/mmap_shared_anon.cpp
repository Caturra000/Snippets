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

    // +Committed_AS:  102832 kB
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
// +Committed_AS:  104820 kB
// ==========
// -MemFree:       103040 kB
// -MemAvailable:  102984 kB
// +Cached:        102328 kB
// +Active:        1152 kB
// +Inactive:      102004 kB
// +Active(anon):  876 kB
// +Inactive(anon):        102280 kB
// +Active(file):  276 kB
// -Inactive(file):        276 kB
// -Dirty: 216 kB
// +AnonPages:     960 kB
// +Mapped:        102636 kB
// +Shmem: 102328 kB
// +KReclaimable:  112 kB
// +Slab:  144 kB
// +SReclaimable:  112 kB
// +PageTables:    144 kB
// +AnonHugePages: 2048 kB
// ==========
// -MemFree:       504 kB
// -MemAvailable:  504 kB
// +Active:        324 kB
// +Active(anon):  324 kB
// +AnonPages:     324 kB

// 如果把测试读过程注释掉，只测试写
// +Committed_AS:  103084 kB
// ==========
// ==========
// -MemFree:       101220 kB
// -MemAvailable:  101220 kB
// +Cached:        102384 kB
// +Active:        136 kB
// +Inactive:      102060 kB
// +Active(anon):  136 kB
// +Inactive(anon):        102060 kB
// +AnonPages:     108 kB
// +Mapped:        102424 kB
// +Shmem: 102384 kB
// +PageTables:    244 kB
// -AnonHugePages: 2048 kB