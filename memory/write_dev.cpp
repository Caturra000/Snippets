#include "meminfo_parser.hpp"

void parse(mp::Meminfo &meminfo) {
    if(mp::meminfo_parse(meminfo)) {
        throw std::runtime_error("error parse");
    }
}

// 使用说明见read_dev.cpp
int main() {
    mp::Meminfo m[2];
    parse(m[0]);

    int fd = ::open("/dev/loop0", O_WRONLY);
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
    return 0;
}

// write: 102400KB
// -MemFree:       203616 kB
// +MemAvailable:  2180 kB
// +Buffers:       102400 kB
// +Cached:        102308 kB
// +Inactive:      204996 kB
// +Inactive(anon):        432 kB
// +Inactive(file):        204564 kB
// +Dirty: 102396 kB
// +AnonPages:     412 kB
// +Mapped:        232 kB
// +KReclaimable:  2464 kB
// +Slab:  2484 kB
// +SReclaimable:  2464 k
