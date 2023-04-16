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
    // for(size_t i = 0; i < len; ++i) {
    //     dummy += ACCESS_ONCE(cur[i]);
    // }

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


// +Committed_AS:  102748 kB
// ==========
// -MemFree:       104196 kB
// +MemAvailable:  484 kB
// +Cached:        104948 kB
// +Active:        852 kB
// +Inactive:      104148 kB
// +Active(anon):  480 kB
// +Active(file):  372 kB
// +Inactive(file):104148 kB
// +Dirty: 1448 kB
// +AnonPages:     556 kB
// +Mapped:        102568 kB
// +KReclaimable:  324 kB
// +Slab:  356 kB
// +SReclaimable:  324 kB
// +KernelStack:   112 kB
// +PageTables:    176 kB
// +AnonHugePages: 8192 kB
// ==========
// -MemFree:       106000 kB
// -MemAvailable:  102892 kB
// +Cached:        2788 kB
// +Active:        102812 kB
// +Inactive:      2968 kB
// +Active(anon):  102708 kB
// +Active(file):  104 kB
// +Inactive(file):2968 kB
// +Dirty: 2788 kB
// +AnonPages:     102992 kB
// -Mapped:        101900 kB


// 如果不测试读，只测试写
// +Committed_AS:  102880 kB
// ==========
// ==========
// -MemFree:       205408 kB
// -MemAvailable:  103072 kB
// +Cached:        102492 kB
// +Active:        102912 kB
// +Inactive:      102320 kB
// +Active(anon):  102912 kB
// +Inactive(file):102320 kB
// +AnonPages:     102760 kB
// +Committed_AS:  3132 kB