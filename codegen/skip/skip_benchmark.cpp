#include <cstring>
#include <cstdint>
#include <algorithm>
#include <ranges>
#include <string_view>
#include <array>
#include <string>
#include <iostream>
#include <vector>
#include <random>
#include <ranges>
#include <benchmark/benchmark.h>

auto make(size_t size) {
    constexpr auto elements = std::array<std::string_view, 4> {
        "null", "true", "false",
        "x" // Unexpected value.
    };
    auto nr_each = size / elements.size();
    auto capacity = nr_each * elements.size();
    std::vector<size_t> cp;
    for(auto i : std::views::iota(0) | std::views::take(capacity)) {
        cp.emplace_back(i % elements.size());
    }
    std::ranges::shuffle(cp, std::mt19937{19260817});
    std::string result;
    for(auto i : cp) result += elements[i];
    return result;
}

// ~std::bit_cast
bool EqBytes4(const char *src, uint32_t target) {
    uint32_t val;
    std::memcpy(&val, src, sizeof(uint32_t));
    return val == target;
}

bool skip_literal_v1(const char *data, size_t &pos,
                  size_t len, uint8_t token) {
    static constexpr uint32_t kNullBin = 0x6c6c756e;
    static constexpr uint32_t kTrueBin = 0x65757274;
    static constexpr uint32_t kFalseBin =
        0x65736c61;  // the binary of 'alse' in false
    auto start = data + pos;
    auto end = data + len + 1;
    switch (token) {
      case 't':
        if (start + 4 < end && EqBytes4(start, kTrueBin)) {
          pos += 4;
          return true;
        };
        break;
      case 'n':
        if (start + 4 < end && EqBytes4(start, kNullBin)) {
          pos += 4;
          return true;
        };
        break;
      case 'f':
        if (start + 5 < end && EqBytes4(start + 1, kFalseBin)) {
          pos += 5;
          return true;
        }
    }
    return false;
}

bool skip_literal_v2(const char *data, size_t &pos,
                     size_t len, uint8_t token) {
  (void) token;
  static constexpr uint32_t kNullBin = 0x6c6c756e;
  static constexpr uint32_t kTrueBin = 0x65757274;
  static constexpr uint32_t kAlseBin = 0x65736c61;  // the binary of 'alse' in false
  static constexpr uint32_t kFalsBin = 0x736c6166;  // the binary of 'fals' in false
  
  auto start = data + pos;
  auto end = data + len + 1;
  if (start + 5 < end) {
      if (EqBytes4(start, kNullBin) || EqBytes4(start, kTrueBin)) {
          pos += 4;
          return true;
      }
      if (EqBytes4(start, kFalsBin) && EqBytes4(start + 1, kAlseBin)) {
          pos += 5;
          return true;
      }
  }
  // slow path
  if (start + 4 < end) {
      if (EqBytes4(start, kNullBin) || EqBytes4(start, kTrueBin)) {
          pos += 4;
          return true;
      }
  }
  return false;
}

static std::string test_string = make(1e4);

template <auto F>
void benchmark_template(auto &state) {
    benchmark::DoNotOptimize(test_string);
    for(auto _ : state) {
        for(size_t cur = 0; cur < test_string.length();) {
            auto result = F(test_string.data(), cur,
                            test_string.length(), test_string[cur]);
            benchmark::DoNotOptimize(result);
            if(!result) cur++;
        }
    }
}

// static std::string simple = make(12);
// template <auto F>
// void test() {
//     int ret = 0;
//     for(size_t cur = 0; cur < simple.length();) {
//         auto result = F(simple.data(), cur, simple.length(), simple[cur]);
//         if(!result) cur++;
//         else ret++;
//     }
//     std::cout << ret << std::endl;
// }
// int main() {
//     std::cout << simple << std::endl;
//     test<skip_literal_v1>();
//     test<skip_literal_v2>();
// }


void BM_skip_literal_v1(benchmark::State& state) {
    benchmark_template<skip_literal_v1>(state);
}

void BM_skip_literal_v2(benchmark::State& state) {
    benchmark_template<skip_literal_v2>(state);
}

BENCHMARK(BM_skip_literal_v1);
BENCHMARK(BM_skip_literal_v2);

BENCHMARK_MAIN();
