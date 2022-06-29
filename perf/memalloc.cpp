#include <bits/stdc++.h>

// 配置选项
// 默认是单测完后再全部回收

// 如果为true，malloc后就不管了，直到程序结束由内核回收内存
constexpr bool IGNORE_FREE = false;
// 如果为true，每次malloc后用完尽快free
constexpr bool FREE_IMMEDIATELY = false;

// 配置选项
// 内存分配的粒度，默认是单次最高1MB，对应于常用数据结构的大小上限
// 总内存用于提高测试的精确度，注意不要过大导致swap，而且由于min step是1字节，调过大会很慢

// step的最大分配值
constexpr size_t _1MB = 1 << 20;
constexpr size_t TOTAL_MEM = 32 * _1MB;

using Pchar = char*;
using PcharArray = Pchar*;

// .bss cannot hold a too large field
// Pchar buf[TOTAL_MEM];

int main() {
    PcharArray buf;
    buf = (Pchar*)::malloc(sizeof(void*) * TOTAL_MEM);

    using namespace std::chrono_literals;
    using namespace std::chrono;
    using Milli = duration<double, std::milli>;
    using Nano = duration<double, std::nano>;
    constexpr auto unit = duration<double, std::nano>(1ms).count();

    Milli elapsed {};

    // tuple: [total, step]
    std::vector<std::tuple<Milli, size_t>> results;

    // 单次分配，每次倍增
    // 首次迭代每次分配1字节，最多每次分配1M
    for(size_t step = 1; step <= _1MB; step <<= 1) {
    // for(size_t step = _1MB; step; step >>= 1) {
        auto start = system_clock::now();

        size_t blockSize = step;
        size_t bufferSize = TOTAL_MEM / blockSize;
        for(size_t cur = 0; cur < bufferSize; cur++) {
            buf[cur] = (Pchar)::malloc(blockSize);
            // forced page-fault
            ::memset(buf[cur], 0x3f, blockSize);

            if constexpr (!IGNORE_FREE && FREE_IMMEDIATELY) {
                ::free(buf[cur]);
            }
        }

        auto end = system_clock::now();

        auto thisStepElapsed = end - start;

        elapsed += thisStepElapsed;

        if constexpr (!IGNORE_FREE && !FREE_IMMEDIATELY) {
            for(size_t cur = 0; cur < bufferSize; cur++) {
                ::free(buf[cur]);
            }
        }

        auto result = std::make_tuple(Milli{thisStepElapsed}, step);
        results.emplace_back(std::move(result));

        std::cout << "===step: " << step << "===" << std::endl;
        std::cout << "total: " << Milli{thisStepElapsed}.count() << "ms" << std::endl;
        std::cout << "average: " << Nano{thisStepElapsed}.count() / bufferSize << "ns" << std::endl;
    }
    auto end = system_clock::now();

    std::cout << "done: " << elapsed.count() << "ms" << std::endl;
}


/*
4750U + 很普通的内存条

===step: 1===
total: 733.99ms
average: 21.8746ns
===step: 2===
total: 207.277ms
average: 12.3547ns
===step: 4===
total: 59.7974ms
average: 7.12841ns
===step: 8===
total: 37.3096ms
average: 8.8953ns
===step: 16===
total: 17.3783ms
average: 8.28662ns
===step: 32===
total: 13.9345ms
average: 13.289ns
===step: 64===
total: 8.9291ms
average: 17.0309ns
===step: 128===
total: 5.8133ms
average: 22.176ns
===step: 256===
total: 5.0796ms
average: 38.7543ns
===step: 512===
total: 8.5451ms
average: 130.388ns
===step: 1024===
total: 7.6744ms
average: 234.204ns
===step: 2048===
total: 6.4521ms
average: 393.805ns
===step: 4096===
total: 5.4119ms
average: 660.632ns
===step: 8192===
total: 4.6599ms
average: 1137.67ns
===step: 16384===
total: 4.4681ms
average: 2181.69ns
===step: 32768===
total: 4.1957ms
average: 4097.36ns
===step: 65536===
total: 4.2497ms
average: 8300.2ns
===step: 131072===
total: 4.8856ms
average: 19084.4ns
===step: 262144===
total: 4.1166ms
average: 32160.9ns
===step: 524288===
total: 4.4606ms
average: 69696.9ns
===step: 1048576===
total: 4.4958ms
average: 140494ns
done: 1153.12ms




不进行free回收，使得下一次malloc增加延迟
===step: 1===
total: 739.839ms
average: 22.0489ns
===step: 2===
total: 358.42ms
average: 21.3635ns
===step: 4===
total: 186.565ms
average: 22.2403ns
===step: 8===
total: 90.4039ms
average: 21.554ns
===step: 16===
total: 51.0341ms
average: 24.335ns
===step: 32===
total: 28.422ms
average: 27.1053ns
===step: 64===
total: 19.8492ms
average: 37.8593ns
===step: 128===
total: 16.1949ms
average: 61.7786ns
===step: 256===
total: 13.1872ms
average: 100.61ns
===step: 512===
total: 12.6719ms
average: 193.358ns
===step: 1024===
total: 11.3588ms
average: 346.643ns
===step: 2048===
total: 10.9967ms
average: 671.185ns
===step: 4096===
total: 11.3755ms
average: 1388.61ns
===step: 8192===
total: 11.0165ms
average: 2689.58ns
===step: 16384===
total: 11.3ms
average: 5517.58ns
===step: 32768===
total: 10.8483ms
average: 10594ns
===step: 65536===
total: 11.1318ms
average: 21741.8ns
===step: 131072===
total: 11.9685ms
average: 46752ns
===step: 262144===
total: 15.4424ms
average: 120644ns
===step: 524288===
total: 14.1782ms
average: 221534ns
===step: 1048576===
total: 12.5691ms
average: 392784ns
done: 1648.77ms




每一次malloc后都进行free
统计耗时也把free算入
TODO 后面把free耗时单独筛选出来
应该是cache最友好的，性能目测起飞
===step: 1===
total: 330.632ms
average: 9.8536ns
===step: 2===
total: 140.144ms
average: 8.35325ns
===step: 4===
total: 69.8152ms
average: 8.32262ns
===step: 8===
total: 36.3592ms
average: 8.66871ns
===step: 16===
total: 22.5485ms
average: 10.752ns
===step: 32===
total: 9.1816ms
average: 8.75626ns
===step: 64===
total: 4.1941ms
average: 7.99961ns
===step: 128===
total: 2.1687ms
average: 8.27293ns
===step: 256===
total: 1.1887ms
average: 9.06906ns
===step: 512===
total: 0.5119ms
average: 7.81097ns
===step: 1024===
total: 0.3711ms
average: 11.3251ns
===step: 2048===
total: 0.4363ms
average: 26.6296ns
===step: 4096===
total: 0.1719ms
average: 20.9839ns
===step: 8192===
total: 0.126ms
average: 30.7617ns
===step: 16384===
total: 0.0447ms
average: 21.8262ns
===step: 32768===
total: 0.0231ms
average: 22.5586ns
===step: 65536===
total: 0.0343ms
average: 66.9922ns
===step: 131072===
total: 0.0092ms
average: 35.9375ns
===step: 262144===
total: 0.0201ms
average: 157.031ns
===step: 524288===
total: 0.0183ms
average: 285.938ns
===step: 1048576===
total: 0.017ms
average: 531.25ns
done: 618.016ms
*/
