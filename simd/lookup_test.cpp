#include "lookup.hpp"
#include <iostream>
#include <vector>
#include <random>

std::mt19937 rng_gen(std::random_device{}());
std::uniform_int_distribution<int> dist_byte(0, 255);

void run_test(size_t data_size) {
    std::cout << "[Test] Data Size: " << data_size << " bytes ... ";

    std::vector<uint8_t> lut(256);
    std::generate(lut.begin(), lut.end(), [&] { return static_cast<uint8_t>(dist_byte(rng_gen)); });

    std::vector<uint8_t> source(data_size);
    std::generate(source.begin(), source.end(), [&] { return static_cast<uint8_t>(dist_byte(rng_gen)); });

    std::vector<uint8_t> expected = source;
    std::vector<uint8_t> actual = source;
    lookup_scalar(expected, lut);
    lookup_ilp(actual, lut);

    bool pass = std::equal(expected.begin(), expected.end(), actual.begin());

    if(pass) {
        std::cout << "\033[32mPASS\033[0m" << std::endl;
    } else {
        std::cout << "\033[31mFAIL\033[0m" << std::endl;

        for(size_t i = 0; i < data_size; ++i) {
            if(expected[i] != actual[i]) {
                std::cout << "  First Error at index [" << i << "]:" << std::endl;
                std::cout << "  Source Value : " << (int)source[i] << std::endl;
                std::cout << "  Expected     : " << (int)lut[source[i]] << " (from scalar)" << std::endl;
                std::cout << "  Actual       : " << (int)actual[i] << " (from simd)" << std::endl;
                break;
            }
        }
        std::exit(1);
    }
}

int main() {
    std::cout << "=== Running SIMD Lookup Verification ===" << std::endl;

    run_test(32);
    run_test(64);
    run_test(1024);
    run_test(1024 * 1024);

    for(int i=0; i<10; ++i) {
        run_test(32 * (i + 1) * 17);
        run_test(dist_byte(rng_gen));
    }

    return 0;
}
