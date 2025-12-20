#include "find_byteset.hpp"

#include <string>
#include <string_view>
#include <array>
#include <cassert>
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm> // for std::copy_n

// ==========================================
// 辅助工具
// ==========================================

// 创建标准的 32 字节 bitmap
std::array<uint8_t, 32> make_byteset(std::string_view chars) {
    std::array<uint8_t, 32> set = {0};
    for (unsigned char c : chars) {
        set[c / 8] |= (1 << (c % 8));
    }
    return set;
}

void print_failure(const std::vector<char>& data, [[maybe_unused]] const std::array<uint8_t, 32>& set,
                   ssize_t expected, ssize_t actual) {
    std::cerr << "\n[FAILED] Expected " << expected << ", got " << actual << "\n";
    size_t start_p = (actual > 10) ? actual - 10 : 0;
    std::cerr << "Data snippet: ";
    for(size_t i = start_p; i < std::min(data.size(), start_p + 20); ++i) {
        std::cerr << (int)(unsigned char)data[i] << " ";
    }
    std::cerr << "\n";
}

// ==========================================
// 测试框架 (保持通用，总是使用 array<u8, 32> 作为通用接口)
// ==========================================

void run_random_fuzz(auto find_byteset_impl, int iterations, int min_char, int max_char) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<size_t> len_dist(0, 2000); 
    
    // 数据生成范围受限 (例如 0-127)
    std::uniform_int_distribution<int> data_dist(min_char, max_char);
    
    // Bitmap 生成范围始终是 0-255 (std::uint8_t 的所有可能)
    // 这样即便测试 ASCII，也能保证如果 Bitmap 设置了高位，程序不会崩，只是因为数据里没有高位字符所以匹配不到
    std::uniform_int_distribution<int> bitmap_byte_dist(0, 255);

    std::cout << "Running " << iterations << " fuzz iterations (Data Range: " 
              << min_char << "-" << max_char << ")..." << std::endl;

    for (int i = 0; i < iterations; ++i) {
        // 1. 总是生成标准的 32字节 Set
        std::array<uint8_t, 32> byteset = {0};
        for (auto& b : byteset) b = bitmap_byte_dist(rng);

        // 2. 生成数据
        size_t len = len_dist(rng);
        std::vector<char> data(len);
        for (auto& c : data) c = static_cast<char>(data_dist(rng));

        // 3. 对比：标量版总是用 32字节 set
        ssize_t scalar_res = find_byteset_scalar(data, byteset);
        
        // 4. SIMD版：通过 wrapper 自动处理参数适配
        ssize_t simd_res = find_byteset_impl(data, byteset);

        if (scalar_res != simd_res) {
            print_failure(data, byteset, scalar_res, simd_res);
            std::exit(1);
        }
        
        if (iterations > 100 && i % (iterations / 10) == 0) std::cout << "." << std::flush;
    }
    std::cout << " DONE!" << std::endl;
}

void run_edge_cases(auto find_byteset_impl, int max_supported_char) {
    std::cout << "Running edge cases..." << std::endl;
    
    // Case 1: Empty
    {
        std::vector<char> empty;
        auto set = make_byteset("abc");
        assert(find_byteset_impl(empty, set) == -1);
    }

    // Case 2: Tail
    {
        std::vector<char> data(65, 'x'); 
        data[64] = 'a';
        auto set = make_byteset("a");
        assert(find_byteset_impl(data, set) == 64);
    }

    // Case 3: Alignment
    {
        std::vector<char> data(64, 'x');
        data[32] = 'a'; 
        auto set = make_byteset("a");
        assert(find_byteset_impl(data, set) == 32);
    }
    
    // Case 4: High bit (仅在支持范围 >= 0xFF 时运行)
    if (max_supported_char >= 0xFF) {
        std::vector<char> data(32, 'a');
        data[10] = (char)0xFF; 
        std::array<uint8_t, 32> set = {0};
        set[0xFF / 8] |= (1 << (0xFF % 8)); 
        
        ssize_t res = find_byteset_impl(data, set);
        if (res != 10) {
            std::cerr << "High bit char failed. Expected 10, got " << res << std::endl;
            std::exit(1);
        }
    }

    std::cout << "Edge cases passed!" << std::endl;
}

// ==========================================
// Main
// ==========================================

int main() {
    // ---------------------------------------------------------
    // 1. 标准 AVX2 (支持 0-255，Set 大小 32)
    // ---------------------------------------------------------
    auto avx2_wrapper = [](const std::vector<char>& data, const std::array<uint8_t, 32>& set) {
        // 直接传
        return find_byteset_avx2(data, set); 
    };

    std::cout << "=== Testing Standard AVX2 (0-255) ===" << std::endl;
    run_edge_cases(avx2_wrapper, 255);
    run_random_fuzz(avx2_wrapper, 100000, 0, 255); 
    std::cout << "\n";


    // ---------------------------------------------------------
    // 2. ASCII AVX2 (仅支持 0-127，Set 大小 16)
    // ---------------------------------------------------------
    // 关键点：这个 Wrapper 负责把 32字节的测试 Set 转换成 16字节的 Set
    auto avx2_ascii128_wrapper = [](const std::vector<char>& data, const std::array<uint8_t, 32>& set32) {
        
        // 适配步骤：提取前 16 个字节 (0-127位的映射)
        std::array<uint8_t, 16> set16;
        std::copy_n(set32.begin(), 16, set16.begin());

        // 调用底层函数 (假设它接受 std::array<uint8_t, 16>)
        return find_byteset_avx2_ascii128(data, set16);
    };

    std::cout << "=== Testing AVX2 ASCII (0-127) ===" << std::endl;
    
    // 告诉 Edge case 仅支持到 127，跳过 0xFF 测试
    run_edge_cases(avx2_ascii128_wrapper, 127);
    
    // 告诉 Fuzz 生成器仅生成 0-127 的字符
    run_random_fuzz(avx2_ascii128_wrapper, 100000, 0, 127);
    std::cout << "\n";


    // 3
    auto avx2_ascii128_transposed_wrapper = [](const std::vector<char>& data, const std::array<uint8_t, 32>& set32) {
        std::array<uint8_t, 16> set16;
        std::copy_n(set32.begin(), 16, set16.begin());
        return find_byteset_avx2_ascii128_transposed(data, set16);
    };
    std::cout << "=== Testing AVX2 ASCII Transposed (0-127) ===" << std::endl;
    run_edge_cases(avx2_ascii128_transposed_wrapper, 127);
    run_random_fuzz(avx2_ascii128_transposed_wrapper, 100000, 0, 127);
    return 0;
}