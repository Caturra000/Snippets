#include "meminfo_parser.hpp"

void parse(mp::Meminfo &meminfo) {
    if(mp::meminfo_parse(meminfo)) {
        throw std::runtime_error("error parse");
    }
}

int main() {
    mp::Meminfo m[2];
    parse(m[0]);

    int fd = ::open("test_write.bin", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if(fd < 0) {
        return 1;
    }
    // 1kB
    char buf[1024];
    constexpr size_t count = 1024 * 100;
    ssize_t n = 0;
    // write 100MB
    for(size_t i = count; i--;) {
        int ret = ::write(fd, buf, sizeof buf);
        if(ret > 0) n += ret;
    }
    parse(m[1]);
    std::cout << "write: " << (n / 1024) << "KB" << std::endl;
    mp::dump_diff(m[0], m[1], 100);
    ::unlink("test_write.bin");
    return 0;
}

// write: 102400KB
// -MemFree:       105620 kB
// -MemAvailable:  2264 kB
// +Cached:        102172 kB
// +Active:        348 kB
// +Inactive:      102060 kB
// +Active(anon):  348 kB
// +Inactive(file):        102060 kB
// +Dirty: 102432 kB
// +AnonPages:     376 kB
// +Mapped:        120 kB
// +KReclaimable:  2596 kB
// +Slab:  2628 kB
// +SReclaimable:  2596 kB
// +AnonHugePages: 10240 kB
