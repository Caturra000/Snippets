#include "meminfo_parser.hpp"

void parse(mp::Meminfo &meminfo) {
    if(mp::meminfo_parse(meminfo)) {
        throw std::runtime_error("error parse");
    }
}

// 这个程序用于测试regular file的读写操作
// 执行前先准备文件
// dd if=/dev/zero of=test_read.bin bs=1048576 count=100
// 并重启主机（因为dd操作已经影响了page cache，而drop_caches指令似乎并不彻底）
int main() {
    mp::Meminfo m[2];
    parse(m[0]);

    int fd = ::open("test_read.bin", O_RDONLY);
    if(fd < 0) {
        return 1;
    }
    // 1kB
    char buf[1024];
    constexpr size_t count = 1024 * 100;
    ssize_t n = 0;
    // read 100MB
    for(size_t i = count; i--;) {
        int ret = ::read(fd, buf, sizeof buf);
        if(ret > 0) n += ret;
    }
    parse(m[1]);
    std::cout << "read: " << (n / 1024) << "KB" << std::endl;
    mp::dump_diff(m[0], m[1], 100);
    return 0;
}

// read: 102400KB
// -MemFree:       108096 kB
// -MemAvailable:  5860 kB
// +Cached:        102236 kB
// +Inactive:      102724 kB
// +Inactive(anon):488 kB
// +Inactive(file):102236 kB
// +AnonPages:     532 kB
// -PageTables:    2684 kB
// -Committed_AS:  98108 kB