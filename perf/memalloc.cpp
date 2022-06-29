#include <bits/stdc++.h>

// step的最大分配值
constexpr size_t _1MB = 1 << 20;
// 自行调整
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

    uint32_t sum = 0;

    // tuple: [total, step]
    std::vector<std::tuple<Milli, size_t>> results;

    // 单次分配，每次倍增
    // 首次迭代每次分配1字节，最多每次分配1M
    for(size_t step = 1; step <= _1MB; step <<= 1) {

        auto start = system_clock::now();

        size_t blockSize = step;
        size_t bufferSize = TOTAL_MEM / blockSize;
        for(size_t cur = 0; cur < bufferSize; cur++) {
            buf[cur] = (Pchar)::malloc(blockSize);
            // forced COW
            ::memset((Pchar)(buf[cur]), 0, blockSize);
            // don't free
        }

        auto end = system_clock::now();

        auto thisStepElapsed = end - start;

        elapsed += thisStepElapsed;

        for(size_t cur = 0; cur < bufferSize; cur++) {
            ::free(buf[cur]);
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
total: 733.253ms
average: 21.8526ns
===step: 2===
total: 207.043ms
average: 12.3407ns
===step: 4===
total: 77.1243ms
average: 9.19393ns
===step: 8===
total: 35.4358ms
average: 8.44855ns
===step: 16===
total: 17.8516ms
average: 8.51231ns
===step: 32===
total: 14.5849ms
average: 13.9092ns
===step: 64===
total: 9.5602ms
average: 18.2346ns
===step: 128===
total: 6.4177ms
average: 24.4816ns
===step: 256===
total: 5.5278ms
average: 42.1738ns
===step: 512===
total: 10.999ms
average: 167.831ns
===step: 1024===
total: 9.3602ms
average: 285.651ns
===step: 2048===
total: 7.9884ms
average: 487.573ns
===step: 4096===
total: 5.7865ms
average: 706.36ns
===step: 8192===
total: 5.5058ms
average: 1344.19ns
===step: 16384===
total: 4.8829ms
average: 2384.23ns
===step: 32768===
total: 4.1197ms
average: 4023.14ns
===step: 65536===
total: 4.3931ms
average: 8580.27ns
===step: 131072===
total: 4.5636ms
average: 17826.6ns
===step: 262144===
total: 4.5459ms
average: 35514.8ns
===step: 524288===
total: 5.3668ms
average: 83856.2ns
===step: 1048576===
total: 5.7235ms
average: 178859ns
done: 1180.03ms
*/
