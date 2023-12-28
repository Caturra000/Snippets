#include <sys/mman.h>
#include <iostream>

// 背景：https://access.redhat.com/solutions/99913
// 测试：/proc/sys/vm/max_map_count
// 默认值：65530
// man proc文档并没有对这个值进行说明
// redhat文档说这个值会限制一个进程mmap的数目，也就是说最多能分出65530个mmap region
// 我目前测试【并不符合】这个结论，起码本机已经分配了700000000+个region了，还没结束，不测了

int main() {

    auto mmap_nofail =[](auto &&mmap_nofail, const auto &...args) {
        auto ret = ::mmap(args...);
        if(ret != MAP_FAILED) return ret;
        if(errno == EINTR) return mmap_nofail(mmap_nofail, args...);
        return MAP_FAILED;
    };
    auto mmap = [&](const auto &...args) { return mmap_nofail(mmap_nofail, args...); };

    size_t count = 0;
    constexpr size_t mmap_size = 1;
    while(MAP_FAILED != mmap(nullptr, mmap_size, PROT_READ, MAP_PRIVATE | MAP_ANON, 0, 0)) {
        count++;
        if(count % 10000000 == 0) {
            std::cout << count << "..." << std::endl;
        }
    }
    std::cout << count << std::endl;
}
