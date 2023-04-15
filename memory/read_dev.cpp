#include "meminfo_parser.hpp"

void parse(mp::Meminfo &meminfo) {
    if(mp::meminfo_parse(meminfo)) {
        throw std::runtime_error("error parse");
    }
}

// 这个程序用于模拟块设备的读写操作
//
// 注意要在root权限下执行
// 执行前操作
// dd if=/dev/zero of=mydisk.img bs=1048576 count=100
// sudo losetup /dev/loop0 mydisk.img
//
// 执行完后记得卸载块设备
// sudo losetup -d /dev/loop0
int main() {
    mp::Meminfo m[2];
    parse(m[0]);

    // 如果没有虚拟块设备，即使这里open成功，后续读入也是0字节
    int fd = ::open("/dev/loop0", O_RDONLY);
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

// 从dd到执行程序都不清理
// read: 102400KB
// -MemFree:       101556 kB
// +MemAvailable:  680 kB
// +Buffers:       102400 kB
// -Cached:        164 kB
// +Active:        488 kB
// +Inactive:      101748 kB
// +Active(file):  488 kB
// +Inactive(file):101748 kB

// dd完成后重启主机，然后执行本程序
// read: 102400KB
// -MemFree:       202292 kB
// +MemAvailable:  2092 kB
// +Buffers:       102400 kB
// +Cached:        101984 kB
// +Inactive:      204960 kB
// +Inactive(anon):        680 kB
// +Inactive(file):        204280 kB
// +AnonPages:     528 kB
// +KReclaimable:  208 kB
// +Slab:  240 kB
// +SReclaimable:  208 kB
// +PageTables:    108 kB