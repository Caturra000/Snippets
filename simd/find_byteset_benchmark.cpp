#include "find_byteset.hpp"

#include <benchmark/benchmark.h>

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

// ============================================================================
// Benchmark Modes
// ============================================================================

enum class Mode {
    NoMatch,       // 扫描整个数组，返回 -1 (测吞吐量上限)
    MatchLast,     // 最后一个字符匹配 (测完整路径+退出逻辑)
    MatchMiddle,   // 中间位置匹配
    MatchFirst1P,  // 前 1% 位置匹配 (测启动延迟/对齐开销)
    Random         // 纯随机内容和位图 (测分支预测/真实场景)
};

std::string mode_name(Mode m) {
    switch(m) {
        case Mode::NoMatch:      return "NoMatch";
        case Mode::MatchLast:    return "MatchLast";
        case Mode::MatchMiddle:  return "MatchMid";
        case Mode::MatchFirst1P: return "Match1%";
        case Mode::Random:       return "Random";
    }
    return "Unknown";
}

// ============================================================================
// Data Generator
// ============================================================================

template <size_t Size, Mode M>
const auto& generate_data() {
    using DataPair = std::pair<std::array<char, Size>, std::array<uint8_t, 32>>;
    
    static const DataPair pair = [] {
        DataPair result;
        auto &[data, byteset] = result;

        std::mt19937 gen{42};
        std::uniform_int_distribution<int> char_dist(0, 255);
        std::uniform_int_distribution<int> bool_dist(0, 1);

        auto in_set = [&](unsigned char c) {
            return (byteset[c / 8] >> (c % 8)) & 1;
        };

        byteset.fill(0);
        if constexpr (M == Mode::Random) {
             for(auto &b : byteset) b = char_dist(gen);
        } else {
             for(int i = 0; i < 32; ++i) byteset[i] = char_dist(gen);
        }

        if constexpr (M == Mode::Random) {
            for(auto &c : data) c = static_cast<char>(char_dist(gen));
        } else {
            for(auto &c : data) {
                unsigned char temp;
                do {
                    temp = static_cast<unsigned char>(char_dist(gen));
                } while(in_set(temp));
                c = static_cast<char>(temp);
            }

            auto insert_match = [&](size_t idx) {
                if(idx >= Size) return;
                unsigned char match;
                do {
                    match = static_cast<unsigned char>(char_dist(gen));
                } while(!in_set(match));
                data[idx] = static_cast<char>(match);
            };

            if constexpr (M == Mode::MatchLast) {
                insert_match(Size - 1);
            } else if constexpr (M == Mode::MatchMiddle) {
                insert_match(Size / 2);
            } else if constexpr (M == Mode::MatchFirst1P) {
                insert_match(std::max<size_t>(0, Size / 100));
            }
            // NoMatch
        }

        return result;
    } ();
    return pair;
}

// ============================================================================
// Benchmark Runner
// ============================================================================

template <size_t Size, Mode M>
void BM_run(benchmark::State &state, auto &&func) {
    const auto& [data, byteset] = generate_data<Size, M>();

    for(auto _ : state) {
        auto res = func(data, byteset);
        benchmark::DoNotOptimize(res);
        benchmark::DoNotOptimize(data.data());
    }

    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(Size) * sizeof(char));
}

template <Mode M, size_t ...Is>
void register_tests_for_mode(std::integer_sequence<size_t, Is...>,
                             std::string alg_name, auto func) {
    (benchmark::RegisterBenchmark(
        (alg_name + "/" + mode_name(M) + "/" + std::to_string(Is)).c_str(),
        [func](benchmark::State& state) {
            BM_run<Is, M>(state, func);
        }
    ), ...);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
    using Sizes = std::integer_sequence<size_t,
        35,
        350,
        3502,
        35023,
        350234
    >;
    Sizes seq;

    auto register_all_modes = [&](std::string name, auto func) {
        register_tests_for_mode<Mode::NoMatch>(seq, name, func);
        register_tests_for_mode<Mode::MatchLast>(seq, name, func);
        register_tests_for_mode<Mode::MatchMiddle>(seq, name, func);
        register_tests_for_mode<Mode::MatchFirst1P>(seq, name, func);
        register_tests_for_mode<Mode::Random>(seq, name, func);
    };

    register_all_modes("avx2",
        [](auto &&rng, const auto &set) { 
            return find_byteset_avx2(rng, set); 
        });

    register_all_modes("scalar",
        [](auto &&rng, const auto &set) { 
            return find_byteset_scalar(rng, set); 
        });

    register_all_modes("std", 
        [](auto &&rng, const auto &set) {
            auto it = std::ranges::find_if(rng, [&](char c) {
                auto uc = static_cast<unsigned char>(c);
                return (set[uc / 8] >> (uc % 8)) & 1;
            });
            if(it == std::ranges::end(rng)) return (ssize_t)-1;
            return (ssize_t)std::distance(std::ranges::begin(rng), it);
        });

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
