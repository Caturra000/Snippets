#include "find_byteset.hpp"

#include <iostream>
#include <vector>
#include <array>
#include <string_view>
#include <random>
#include <functional>
#include <algorithm>
#include <iomanip>
#include <cassert>

// =========================================================
// 1. 基础定义与工具
// =========================================================

// 统一接口：所有被测函数都将被包装成这种形式
// data: 输入数据
// bitmap32: 总是传入 32字节 的完整 bitmap，具体函数根据自身能力截取使用
using TestFunc = std::function<ssize_t(const std::vector<char>&, const std::array<uint8_t, 32>&)>;

struct TestConfig {
    std::string name;       // 测试名称
    int min_char;           // 输入字符串生成的最小字符值 (例如 -128)
    int max_char;           // 输入字符串生成的最大字符值 (例如 127)
    bool mask_high_bitmap;  // 是否强制将 Bitmap 的高16字节视为无效 (针对只支持 ASCII 的 Bitmap)
};

// 打印错误时的辅助工具
void report_error(const std::vector<char>& data, const std::array<uint8_t, 32>& set,
                  ssize_t expected, ssize_t actual) {
    (void) set;
    std::cerr << "\n=========================================\n";
    std::cerr << "[TEST FAILED]\n";
    std::cerr << "  Expected: " << expected << "\n";
    std::cerr << "  Actual:   " << actual << "\n";
    
    // 确定打印范围
    ssize_t center = (actual != -1) ? actual : (expected != -1 ? expected : 0);
    ssize_t start = std::max((ssize_t)0, center - 8);
    ssize_t end = std::min((ssize_t)data.size(), center + 8);

    std::cerr << "  Context (Data around index " << center << "):\n  ";
    for (ssize_t i = start; i < end; ++i) {
        std::cerr << std::setw(4) << i;
    }
    std::cerr << "\n  ";
    for (ssize_t i = start; i < end; ++i) {
        // 打印字符的整数值 (Hex)，处理负数
        std::cerr << std::hex << std::setw(4) << (static_cast<int>(data[i]) & 0xFF) << std::dec;
    }
    std::cerr << "\n";
    if (actual != -1 && actual >= start && actual < end) {
        std::cerr << "  " << std::string((actual - start) * 4 + 2, ' ') << "^ (Actual)\n";
    }
    std::cerr << "=========================================\n";
}

// =========================================================
// 2. 标量参考实现 (The Oracle)
// =========================================================

// 绝对正确的参考实现
ssize_t find_byteset_scalar_ref(const std::vector<char>& data, const std::array<uint8_t, 32>& bitmap) {
    for (size_t i = 0; i < data.size(); ++i) {
        uint8_t u_char = static_cast<uint8_t>(data[i]);
        if (bitmap[u_char / 8] & (1 << (u_char % 8))) {
            return static_cast<ssize_t>(i);
        }
    }
    return -1;
}

// =========================================================
// 3. Fuzz 测试引擎
// =========================================================

class FuzzTester {
    std::mt19937 rng{std::random_device{}()};
    
public:
    void run(const TestConfig& config, TestFunc target_impl, int iterations = 100000) {
        std::cout << "[RUNNING] " << std::left << std::setw(40) << config.name 
                  << " Range:[" << config.min_char << ", " << config.max_char << "]"
                  << "\tMaskHigh:" << (config.mask_high_bitmap ? "YES\t" : "NO\t")
                  << " ... " << std::flush;

        std::uniform_int_distribution<int> char_dist(config.min_char, config.max_char);
        std::uniform_int_distribution<size_t> len_dist(0, 1024);
        std::uniform_int_distribution<int> byte_dist(0, 255);

        for (int i = 0; i < iterations; ++i) {
            // 1. 生成 Bitmap
            std::array<uint8_t, 32> bitmap;
            for (auto& b : bitmap) b = byte_dist(rng);

            // [关键逻辑]
            // 如果测试的目标函数只支持 ASCII bitmap，
            // 那么对于参考实现来说，高位部分必须全是 0，
            // 否则当 data 中出现负数时，参考实现会匹配，而目标函数物理上无法匹配，导致测试误报。
            if (config.mask_high_bitmap) {
                std::fill(bitmap.begin() + 16, bitmap.end(), 0);
            }

            // 2. 生成数据
            size_t len = len_dist(rng);
            std::vector<char> data(len);
            for (auto& c : data) c = static_cast<char>(char_dist(rng));

            // 3. 执行对比
            // 标量版使用经过 mask 处理的 bitmap，确保逻辑一致
            ssize_t expected = find_byteset_scalar_ref(data, bitmap);
            ssize_t actual = target_impl(data, bitmap);

            if (expected != actual) {
                report_error(data, bitmap, expected, actual);
                std::exit(EXIT_FAILURE);
            }
        }
        std::cout << "PASSED" << std::endl;
    }
};

// =========================================================
// 4. Main 入口与用例配置
// =========================================================

int main() {
    FuzzTester tester;

    // ----------------------------------------------------------------------------------
    // Case 1: 标准 AVX2
    // 特性：输入全范围 (-128~127)，Bitmap 全范围 (0~255)
    // ----------------------------------------------------------------------------------
    TestFunc wrapper_avx2 = [](const auto& data, const auto& set32) {
        return find_byteset_avx2(data, set32);
    };

    tester.run({
        .name = "Standard AVX2 (Full Range)",
        .min_char = -128,
        .max_char = 127,
        .mask_high_bitmap = false 
    }, wrapper_avx2);


    // ----------------------------------------------------------------------------------
    // Case 2: 纯 ASCII 场景
    // 特性：输入仅 0~127，Bitmap 仅需 0~127
    // ----------------------------------------------------------------------------------
    TestFunc wrapper_avx2_ascii = [](const auto& data, const auto& set32) {
        // 适配：截断 bitmap 为 16 字节传给底层
        std::array<uint8_t, 16> set16;
        std::copy_n(set32.begin(), 16, set16.begin());
        constexpr avx2_ascii128_config config {.overflow = false};
        return find_byteset_avx2_ascii128<config>(data, set16);
    };

    tester.run({
        .name = "ASCII AVX2 (ASCII Input Only)",
        .min_char = 0,    // 仅生成 ASCII 数据
        .max_char = 127,
        .mask_high_bitmap = true // 即使生成器产生了高位 bitmap，也要抹去，因为底层不支持
    }, wrapper_avx2_ascii);


    // ----------------------------------------------------------------------------------
    // Case 3: Transposed ASCII (混合场景) - 重点！
    // 特性：输入全范围 (-128~127)，但 Bitmap 仅支持 0~127
    // 目的：验证当输入包含负数/非法字符时，函数能正确跳过它们而不是崩溃或误报
    // ----------------------------------------------------------------------------------
    TestFunc wrapper_transposed = [](const auto& data, const auto& set32) {
        std::array<uint8_t, 16> set16;
        std::copy_n(set32.begin(), 16, set16.begin());
        constexpr avx2_ascii128_transposed_config config {.transposed = false};
        return find_byteset_avx2_ascii128_transposed<config>(data, set16);
    };

    tester.run({
        .name = "Transposed (Dirty Input, ASCII Set)",
        .min_char = -128, // 生成包含负数/高位的数据
        .max_char = 127,
        .mask_high_bitmap = true // 关键：告诉测试机，Bitmap 的高位是无效的
                                 // 这样如果 Data 含有 -1 (0xFF)，Scalar 版查表发现 set[255]==0，不匹配
                                 // SIMD 版查不到，不匹配 -> 结果一致 -> 通过
    }, wrapper_transposed);

    // ----------------------------------------------------------------------------------
    // Case 4: ASCII (混合场景)
    // 特性：输入全范围 (-128~127)，但 Bitmap 仅支持 0~127
    // 目的：验证当输入包含负数/非法字符时，函数能正确跳过它们而不是崩溃或误报
    // ----------------------------------------------------------------------------------
    TestFunc wrapper_avx2_ascii_overflow = [](const auto& data, const auto& set32) {
        std::array<uint8_t, 16> set16;
        std::copy_n(set32.begin(), 16, set16.begin());
        constexpr avx2_ascii128_config config {.overflow = true};
        return find_byteset_avx2_ascii128<config>(data, set16);
    };

    tester.run({
        .name = "ASCII AVX2 (ASCII Input + overflow)",
        .min_char = -128, // 生成包含负数/高位的数据
        .max_char = 127,
        .mask_high_bitmap = true // 关键：告诉测试机，Bitmap 的高位是无效的
                                 // 这样如果 Data 含有 -1 (0xFF)，Scalar 版查表发现 set[255]==0，不匹配
                                 // SIMD 版查不到，不匹配 -> 结果一致 -> 通过
    }, wrapper_avx2_ascii_overflow);

    return 0;
}