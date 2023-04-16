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

// diff m[1] -> m[2]
// -MemFree:       4236 kB
// -MemAvailable:  2628 kB
// +Cached:        2236 kB
// +Active:        1204 kB
// +Inactive:      1168 kB
// +Active(anon):  804 kB
// +Active(file):  400 kB
// +Inactive(file):1168 kB
// +Dirty: 448 kB
// +AnonPages:     772 kB
// +Mapped:        168 kB
// +Slab:  256 kB
// +SUnreclaim:    180 kB
// +Committed_AS:  772 kB
// +AnonHugePages: 8192 kB

// diff m[2] -> m[3]
// -MemFree:       102108 kB
// -MemAvailable:  102108 kB
// +Active:        102672 kB
// +Active(anon):  102672 kB
// +AnonPages:     102572 kB
// +PageTables:    124 kB
// +AnonHugePages: 24576 kB
