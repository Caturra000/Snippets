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
        "t" // Unexpected value.
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
  else if (start + 4 < end) {
      if (EqBytes4(start, kNullBin) || EqBytes4(start, kTrueBin)) {
          pos += 4;
          return true;
      }
  }
  return false;
}

template <auto F>
void benchmark_template(auto &state, size_t nr_token) {
    std::string test_string = make(nr_token);
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

/*
$ perf stat -M PipelineL1
 Performance counter stats for './a.out':

    74,722,674,738      de_src_op_disp.all               #      5.8 %  bad_speculation          (50.01%)
    29,557,874,567      ls_not_halted_cyc                #     36.3 %  retiring                 (50.01%)
    64,382,628,404      ex_ret_ops                                                              (50.01%)
        87,109,879      de_no_dispatch_per_slot.smt_contention #      0.0 %  smt_contention           (50.00%)
    29,522,675,914      ls_not_halted_cyc                                                       (50.00%)
    99,571,510,280      de_no_dispatch_per_slot.no_ops_from_frontend #     56.2 %  frontend_bound           (49.99%)
    29,508,518,726      ls_not_halted_cyc                                                       (49.99%)
     2,964,589,640      de_no_dispatch_per_slot.backend_stalls #      1.7 %  backend_bound            (75.00%)
    29,528,187,809      ls_not_halted_cyc                                                       (75.00%)
*/
void BM_skip_literal_v1(benchmark::State& state) {
    benchmark_template<skip_literal_v1>(state, state.range(0));
}

/*
$ perf stat -M PipelineL1
 Performance counter stats for './a.out':

   120,682,363,513      de_src_op_disp.all               #      6.9 %  bad_speculation          (50.00%)
    35,017,028,479      ls_not_halted_cyc                #     50.6 %  retiring                 (50.00%)
   106,269,993,747      ex_ret_ops                                                              (50.00%)
       172,887,357      de_no_dispatch_per_slot.smt_contention #      0.1 %  smt_contention           (49.99%)
    35,004,281,404      ls_not_halted_cyc                                                       (49.99%)
    74,874,655,398      de_no_dispatch_per_slot.no_ops_from_frontend #     35.7 %  frontend_bound           (50.00%)
    34,996,749,676      ls_not_halted_cyc                                                       (50.00%)
    14,415,385,833      de_no_dispatch_per_slot.backend_stalls #      6.9 %  backend_bound            (75.01%)
    35,004,379,924      ls_not_halted_cyc                                                       (75.01%)
*/
void BM_skip_literal_v2(benchmark::State& state) {
    benchmark_template<skip_literal_v2>(state, state.range(0));
}

BENCHMARK(BM_skip_literal_v1)->Range(8, 1<<20);
BENCHMARK(BM_skip_literal_v2)->Range(8, 1<<20);

BENCHMARK_MAIN();
