#include "find_byteset.hpp"

/// For test.
#include <string>
#include <string_view>
#include <array>
#include <cassert>
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <tuple>

// Helper to create bitmap from string
std::array<uint8_t, 32> make_byteset(std::string_view chars) {
    std::array<uint8_t, 32> set = {0};
    for (unsigned char c : chars) {
        set[c / 8] |= (1 << (c % 8));
    }
    return set;
}

// ==========================================
// Test Harness
// ==========================================

void print_failure(const std::vector<char>& data, [[maybe_unused]] const std::array<uint8_t, 32>& set,
                   ssize_t expected, ssize_t actual) {
    std::cerr << "\n[FAILED] Expected " << expected << ", got " << actual << "\n";
    std::cerr << "Data Size: " << data.size() << "\n";
    std::cerr << "Data (First 64): ";
    for(size_t i=0; i<std::min(data.size(), size_t(64)); ++i) std::cerr << data[i];
    std::cerr << "\n";
}

void run_random_fuzz(int iterations) {
    std::mt19937 rng(42);
    // Random length up to 2000 to cover multiple SIMD blocks + scalar tail
    std::uniform_int_distribution<size_t> len_dist(0, 2000); 
    std::uniform_int_distribution<int> char_dist(0, 255);
    // Probability of a char being in the set
    std::bernoulli_distribution set_density(0.1); 

    std::cout << "Running " << iterations << " fuzz iterations..." << std::endl;

    for (int i = 0; i < iterations; ++i) {
        // 1. Generate Random Bitmap
        std::array<uint8_t, 32> byteset = {0};
        for (auto& b : byteset) b = char_dist(rng);

        // 2. Generate Random Data
        size_t len = len_dist(rng);
        std::vector<char> data(len);
        for (auto& c : data) c = static_cast<char>(char_dist(rng));

        // 3. Compare
        ssize_t scalar_res = find_byteset_scalar(data, byteset);
        ssize_t simd_res = find_byteset_avx2(data, byteset);

        if (scalar_res != simd_res) {
            print_failure(data, byteset, scalar_res, simd_res);
            std::exit(1);
        }
        
        if (i % (iterations / 10) == 0) std::cout << "." << std::flush;
    }
    std::cout << " DONE!" << std::endl;
}

void run_edge_cases() {
    std::cout << "Running edge cases..." << std::endl;
    
    // Test 1: Empty range
    {
        std::vector<char> empty;
        auto set = make_byteset("abc");
        assert(find_byteset_avx2(empty, set) == -1);
    }

    // Test 2: Target at very end (Scalar tail test)
    {
        // 32 * 2 + 1 bytes. SIMD handles 64, Scalar handles 1.
        std::vector<char> data(65, 'x'); 
        data[64] = 'a';
        auto set = make_byteset("a");
        assert(find_byteset_avx2(data, set) == 64);
    }

    // Test 3: Target exactly at lane boundary (Alignment logic)
    {
        std::vector<char> data(64, 'x');
        data[32] = 'a'; // First char of second lane
        auto set = make_byteset("a");
        assert(find_byteset_avx2(data, set) == 32);
    }
    
    // Test 4: High bit characters (Negative char test)
    {
        std::vector<char> data(32, 'a');
        data[10] = (char)0xFF; // -1
        // Manually set bit for 0xFF
        std::array<uint8_t, 32> set = {0};
        set[0xFF / 8] |= (1 << (0xFF % 8)); // Last bit of last byte
        
        ssize_t res = find_byteset_avx2(data, set);
        if (res != 10) {
            std::cerr << "High bit char failed. Expected 10, got " << res << std::endl;
            std::exit(1);
        }
    }

    // Test 5: Even vs Odd table parity check
    {
        // Char 7 (0000 0111) -> Bit3=0 -> Even
        // Char 8 (0000 1000) -> Bit3=1 -> Odd
        std::vector<char> data = {7, 8};
        
        std::array<uint8_t, 32> set_for_7 = {0}; set_for_7[0] = (1<<7);
        assert(find_byteset_avx2(data, set_for_7) == 0);
        
        std::array<uint8_t, 32> set_for_8 = {0}; set_for_8[1] = (1<<0);
        assert(find_byteset_avx2(data, set_for_8) == 1);
    }

    std::cout << "Edge cases passed!" << std::endl;
}

int main() {
    run_edge_cases();
    run_random_fuzz(100000); // 100k iterations
    return 0;
}
