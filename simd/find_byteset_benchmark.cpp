#include "find_byteset.hpp"

#include <benchmark/benchmark.h>

#include <string>
#include <string_view>
#include <array>
#include <cassert>
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <tuple>
#include <algorithm>

// ============================================================================
// Benchmark Modes (原有，保持不变)
// ============================================================================

enum class Mode {
    NoMatch,       // 扫描整个数组，返回 -1 (测吞吐量上限)
    MatchLast,     // 最后一个字符匹配 (测完整路径+退出逻辑)
    MatchMiddle,   // 中间位置匹配
    MatchFirst1P,  // 前 1% 位置匹配 (测启动延迟/对齐开销)
    Random         // 纯随机内容和位图 (测分支预测/真实场景)
};

// ============================================================================
// 新增配置枚举
// ============================================================================

// 字符范围配置
enum class CharRange {
    Full,       // 0-255 (默认)
    Ascii,      // 0-127
    HighAscii,  // 128-255
    Low64,      // 0-63
    Printable,  // 32-126
};

// 预定义字符集 (固定 bitmap)
enum class Preset {
    Random,      // 随机生成 (默认)
    Json,        // JSON 特殊字符: " \ { } [ ] : ,
    Html,        // HTML 特殊字符: < > & " '
    Whitespace,  // 空白字符: space, tab, \n, \r
    LineBreak,   // 仅换行: \n \r
    Digits,      // 数字: 0-9
    Vowels,      // 元音: aeiouAEIOU
    ControlChars,// 控制字符: 0-31
};

// 字符集大小 (控制 byteset 中有效字符数目)
enum class SetSize {
    N1  = 1,
    N2  = 2,
    N4  = 4,
    N8  = 8,
    N16 = 16,
    N32 = 32,
    Full = 0,  // 随机填充 (~128个)
};

// 匹配概率 (用于概率模式)
enum class Prob {
    P0_001,  // 0.001%
    P0_01,   // 0.01%
    P0_1,    // 0.1%
    P1,      // 1%
    P5,      // 5%
    P10,     // 10%
    P25,     // 25%
    P50,     // 50%
};

// ============================================================================
// 名称转换函数
// ============================================================================

std::string mode_name(Mode m) {
    switch(m) {
        case Mode::NoMatch:      return "NoMatch";
        case Mode::MatchLast:    return "MatchLast";
        case Mode::MatchMiddle:  return "MatchMid";
        case Mode::MatchFirst1P: return "Match1%";
        case Mode::Random:       return "Random";
    }
    return "?";
}

std::string range_name(CharRange r) {
    switch(r) {
        case CharRange::Full:      return "0-255";
        case CharRange::Ascii:     return "0-127";
        case CharRange::HighAscii: return "128-255";
        case CharRange::Low64:     return "0-63";
        case CharRange::Printable: return "print";
    }
    return "?";
}

std::string preset_name(Preset p) {
    switch(p) {
        case Preset::Random:       return "rand";
        case Preset::Json:         return "json";
        case Preset::Html:         return "html";
        case Preset::Whitespace:   return "ws";
        case Preset::LineBreak:    return "lf";
        case Preset::Digits:       return "digit";
        case Preset::Vowels:       return "vowel";
        case Preset::ControlChars: return "ctrl";
    }
    return "?";
}

std::string setsize_name(SetSize s) {
    if(s == SetSize::Full) return "full";
    return std::to_string(static_cast<int>(s)) + "ch";
}

std::string prob_name(Prob p) {
    switch(p) {
        case Prob::P0_001: return "0.001%";
        case Prob::P0_01:  return "0.01%";
        case Prob::P0_1:   return "0.1%";
        case Prob::P1:     return "1%";
        case Prob::P5:     return "5%";
        case Prob::P10:    return "10%";
        case Prob::P25:    return "25%";
        case Prob::P50:    return "50%";
    }
    return "?";
}

constexpr double prob_value(Prob p) {
    switch(p) {
        case Prob::P0_001: return 0.00001;
        case Prob::P0_01:  return 0.0001;
        case Prob::P0_1:   return 0.001;
        case Prob::P1:     return 0.01;
        case Prob::P5:     return 0.05;
        case Prob::P10:    return 0.1;
        case Prob::P25:    return 0.25;
        case Prob::P50:    return 0.5;
    }
    return 0.5;
}

// ============================================================================
// Byteset 辅助函数
// ============================================================================

inline void set_bit(std::array<uint8_t, 32>& bs, unsigned char c) {
    bs[c / 8] |= (1u << (c % 8));
}

inline bool test_bit(const std::array<uint8_t, 32>& bs, unsigned char c) {
    return (bs[c / 8] >> (c % 8)) & 1;
}

constexpr std::pair<int, int> get_char_range(CharRange r) {
    switch(r) {
        case CharRange::Ascii:     return {0, 127};
        case CharRange::HighAscii: return {128, 255};
        case CharRange::Low64:     return {0, 63};
        case CharRange::Printable: return {32, 126};
        default:                   return {0, 255};
    }
}

// 获取预设字符集的字符列表
inline std::vector<unsigned char> get_preset_chars(Preset p) {
    switch(p) {
        case Preset::Json:
            return {'"', '\\', '{', '}', '[', ']', ':', ','};
        case Preset::Html:
            return {'<', '>', '&', '"', '\''};
        case Preset::Whitespace:
            return {' ', '\t', '\n', '\r'};
        case Preset::LineBreak:
            return {'\n', '\r'};
        case Preset::Digits:
            return {'0','1','2','3','4','5','6','7','8','9'};
        case Preset::Vowels:
            return {'a','e','i','o','u','A','E','I','O','U'};
        case Preset::ControlChars: {
            std::vector<unsigned char> v;
            for(int i = 0; i < 32; ++i) v.push_back(i);
            return v;
        }
        default:
            return {};
    }
}

inline std::array<uint8_t, 32> preset_to_byteset(Preset p) {
    std::array<uint8_t, 32> bs{};
    for(auto c : get_preset_chars(p)) set_bit(bs, c);
    return bs;
}

// ============================================================================
// 编译时字符集 (用于测试编译器优化的 if(ch == ...) 版本)
// ============================================================================

template<char... Chars>
struct StaticCharset {
    static constexpr size_t size = sizeof...(Chars);
    static constexpr std::array<char, sizeof...(Chars)> chars = {Chars...};
    
    // 编译时展开的 || 链
    static constexpr bool contains(char c) {
        return ((c == Chars) || ...);
    }
    
    // 转换为 byteset (用于对比测试)
    static constexpr std::array<uint8_t, 32> to_byteset() {
        std::array<uint8_t, 32> bs{};
        ((bs[static_cast<unsigned char>(Chars) / 8] |= 
            (1u << (static_cast<unsigned char>(Chars) % 8))), ...);
        return bs;
    }
};

// 预定义编译时字符集 (与 Preset 对应)
using JsonCharsStatic = StaticCharset<'"', '\\', '{', '}', '[', ']', ':', ','>;
using HtmlCharsStatic = StaticCharset<'<', '>', '&', '"', '\''>;
using WsCharsStatic   = StaticCharset<' ', '\t', '\n', '\r'>;
using LfCharsStatic   = StaticCharset<'\n', '\r'>;
using DigitCharsStatic = StaticCharset<'0','1','2','3','4','5','6','7','8','9'>;
using VowelCharsStatic = StaticCharset<'a','e','i','o','u','A','E','I','O','U'>;

// ============================================================================
// 标量查找实现 - 不同版本用于对比
// ============================================================================

// 版本1: 编译时已知字符集 (if chain, 编译器可充分优化)
template<typename Charset>
ssize_t find_chars_static_ct(std::ranges::range auto&& rng) {
    ssize_t i = 0;
    for(auto c : rng) {
        if(Charset::contains(c)) return i;
        ++i;
    }
    return -1;
}

// 版本2: 运行时小字符集 - 循环比较 (编译器可能展开)
template<size_t N>
ssize_t find_chars_loop_rt(std::ranges::range auto&& rng, 
                           const std::array<char, N>& chars) {
    ssize_t i = 0;
    for(auto c : rng) {
        for(size_t j = 0; j < N; ++j) {
            if(c == chars[j]) return i;
        }
        ++i;
    }
    return -1;
}

// 版本3: 运行时小字符集 - 使用 std::find
template<size_t N>
ssize_t find_chars_stdfind_rt(std::ranges::range auto&& rng,
                              const std::array<char, N>& chars) {
    ssize_t i = 0;
    for(auto c : rng) {
        if(std::find(chars.begin(), chars.end(), c) != chars.end()) return i;
        ++i;
    }
    return -1;
}

// 版本4: 编译时字符集生成 bitmap，运行时用 bitmap 查找
template<typename Charset>
ssize_t find_chars_bitmap_ct(std::ranges::range auto&& rng) {
    static constexpr auto byteset = Charset::to_byteset();
    ssize_t i = 0;
    for(auto c : rng) {
        auto uc = static_cast<unsigned char>(c);
        if((byteset[uc / 8] >> (uc % 8)) & 1) return i;
        ++i;
    }
    return -1;
}

// ============================================================================
// Data Generator - 原有版本 (保持兼容)
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
            // NoMatch: 不插入
        }

        return result;
    } ();
    return pair;
}

// ============================================================================
// Data Generator - 带字符范围
// ============================================================================

template <size_t Size, Mode M, CharRange Range>
const auto& generate_data_ranged() {
    using DataPair = std::pair<std::array<char, Size>, std::array<uint8_t, 32>>;
    
    static const DataPair pair = [] {
        DataPair result;
        auto &[data, byteset] = result;

        std::mt19937 gen{42};
        auto [rmin, rmax] = get_char_range(Range);
        std::uniform_int_distribution<int> char_dist(rmin, rmax);

        byteset.fill(0);
        // 在指定范围内随机设置约一半的位
        int range_size = rmax - rmin + 1;
        for(int i = 0; i < range_size / 2; ++i) {
            set_bit(byteset, static_cast<unsigned char>(char_dist(gen)));
        }

        auto in_set = [&](unsigned char c) { return test_bit(byteset, c); };

        if constexpr (M == Mode::Random) {
            for(auto &c : data) c = static_cast<char>(char_dist(gen));
        } else {
            for(auto &c : data) {
                unsigned char temp;
                int tries = 0;
                do { 
                    temp = static_cast<unsigned char>(char_dist(gen)); 
                } while(in_set(temp) && ++tries < 1000);
                c = static_cast<char>(temp);
            }

            auto insert_match = [&](size_t idx) {
                if(idx >= Size) return;
                for(int c = rmin; c <= rmax; ++c) {
                    if(in_set(c)) { data[idx] = static_cast<char>(c); return; }
                }
            };

            if constexpr (M == Mode::MatchLast) insert_match(Size - 1);
            else if constexpr (M == Mode::MatchMiddle) insert_match(Size / 2);
            else if constexpr (M == Mode::MatchFirst1P) insert_match(Size / 100);
        }

        return result;
    }();
    return pair;
}

// ============================================================================
// Data Generator - 带预设字符集
// ============================================================================

template <size_t Size, Mode M, Preset P>
const auto& generate_data_preset() {
    using DataPair = std::pair<std::array<char, Size>, std::array<uint8_t, 32>>;
    
    static const DataPair pair = [] {
        DataPair result;
        auto &[data, byteset] = result;

        std::mt19937 gen{42};
        std::uniform_int_distribution<int> char_dist(0, 255);
        
        if constexpr (P == Preset::Random) {
            byteset.fill(0);
            for(int i = 0; i < 32; ++i) byteset[i] = char_dist(gen);
        } else {
            byteset = preset_to_byteset(P);
        }

        auto in_set = [&](unsigned char c) { return test_bit(byteset, c); };
        auto preset_chars = get_preset_chars(P);

        if constexpr (M == Mode::Random) {
            for(auto &c : data) c = static_cast<char>(char_dist(gen));
        } else {
            for(auto &c : data) {
                unsigned char temp;
                int tries = 0;
                do { 
                    temp = static_cast<unsigned char>(char_dist(gen)); 
                } while(in_set(temp) && ++tries < 1000);
                c = static_cast<char>(temp);
            }

            auto insert_match = [&](size_t idx) {
                if(idx >= Size) return;
                if(!preset_chars.empty()) {
                    std::uniform_int_distribution<size_t> idx_dist(0, preset_chars.size()-1);
                    data[idx] = static_cast<char>(preset_chars[idx_dist(gen)]);
                } else {
                    for(int c = 0; c < 256; ++c) {
                        if(in_set(c)) { data[idx] = static_cast<char>(c); return; }
                    }
                }
            };

            if constexpr (M == Mode::MatchLast) insert_match(Size - 1);
            else if constexpr (M == Mode::MatchMiddle) insert_match(Size / 2);
            else if constexpr (M == Mode::MatchFirst1P) insert_match(Size / 100);
        }

        return result;
    }();
    return pair;
}

// ============================================================================
// Data Generator - 带概率
// ============================================================================

template <size_t Size, Prob P>
const auto& generate_data_prob() {
    using DataPair = std::pair<std::array<char, Size>, std::array<uint8_t, 32>>;
    
    static const DataPair pair = [] {
        DataPair result;
        auto &[data, byteset] = result;

        std::mt19937 gen{42};
        std::uniform_int_distribution<int> char_dist(0, 255);
        std::uniform_real_distribution<double> prob_dist(0.0, 1.0);

        byteset.fill(0);
        for(int i = 0; i < 32; ++i) byteset[i] = char_dist(gen);

        auto in_set = [&](unsigned char c) { return test_bit(byteset, c); };

        // 收集匹配/不匹配字符
        std::vector<unsigned char> match_chars, nomatch_chars;
        for(int c = 0; c < 256; ++c) {
            if(in_set(c)) match_chars.push_back(c);
            else nomatch_chars.push_back(c);
        }

        // 确保两个集合非空
        if(match_chars.empty()) { match_chars.push_back(0); set_bit(byteset, 0); }
        if(nomatch_chars.empty()) nomatch_chars.push_back(255);

        double match_prob = prob_value(P);
        std::uniform_int_distribution<size_t> match_idx(0, match_chars.size()-1);
        std::uniform_int_distribution<size_t> nomatch_idx(0, nomatch_chars.size()-1);

        for(auto &c : data) {
            if(prob_dist(gen) < match_prob) {
                c = static_cast<char>(match_chars[match_idx(gen)]);
            } else {
                c = static_cast<char>(nomatch_chars[nomatch_idx(gen)]);
            }
        }

        return result;
    }();
    return pair;
}

// ============================================================================
// Data Generator - 带字符集大小限制
// ============================================================================

template <size_t Size, Mode M, SetSize S, CharRange Range = CharRange::Full>
const auto& generate_data_sized() {
    using DataPair = std::pair<std::array<char, Size>, std::array<uint8_t, 32>>;
    
    static const DataPair pair = [] {
        DataPair result;
        auto &[data, byteset] = result;

        std::mt19937 gen{42};
        auto [rmin, rmax] = get_char_range(Range);
        std::uniform_int_distribution<int> char_dist(rmin, rmax);

        byteset.fill(0);
        
        if constexpr (S == SetSize::Full) {
            for(int i = 0; i < 32; ++i) byteset[i] = char_dist(gen) & 0xFF;
        } else {
            constexpr int num_chars = static_cast<int>(S);
            for(int i = 0; i < num_chars; ) {
                unsigned char c = static_cast<unsigned char>(char_dist(gen));
                if(!test_bit(byteset, c)) {
                    set_bit(byteset, c);
                    ++i;
                }
            }
        }

        auto in_set = [&](unsigned char c) { return test_bit(byteset, c); };

        if constexpr (M == Mode::Random) {
            for(auto &c : data) c = static_cast<char>(char_dist(gen));
        } else {
            for(auto &c : data) {
                unsigned char temp;
                int tries = 0;
                do { 
                    temp = static_cast<unsigned char>(char_dist(gen)); 
                } while(in_set(temp) && ++tries < 1000);
                c = static_cast<char>(temp);
            }

            auto insert_match = [&](size_t idx) {
                if(idx >= Size) return;
                for(int c = rmin; c <= rmax; ++c) {
                    if(in_set(c)) { data[idx] = static_cast<char>(c); return; }
                }
            };

            if constexpr (M == Mode::MatchLast) insert_match(Size - 1);
            else if constexpr (M == Mode::MatchMiddle) insert_match(Size / 2);
            else if constexpr (M == Mode::MatchFirst1P) insert_match(Size / 100);
        }

        return result;
    }();
    return pair;
}

// ============================================================================
// Data Generator - 为编译时字符集生成数据
// ============================================================================

template <size_t Size, Mode M, typename Charset>
const auto& generate_data_for_static_charset() {
    using DataPair = std::pair<std::array<char, Size>, std::array<uint8_t, 32>>;
    
    static const DataPair pair = [] {
        DataPair result;
        auto &[data, byteset] = result;

        byteset = Charset::to_byteset();

        std::mt19937 gen{42};
        std::uniform_int_distribution<int> char_dist(0, 255);

        auto in_set = [&](unsigned char c) { return test_bit(byteset, c); };

        if constexpr (M == Mode::Random) {
            for(auto &c : data) c = static_cast<char>(char_dist(gen));
        } else {
            for(auto &c : data) {
                unsigned char temp;
                int tries = 0;
                do { 
                    temp = static_cast<unsigned char>(char_dist(gen)); 
                } while(in_set(temp) && ++tries < 1000);
                c = static_cast<char>(temp);
            }

            auto insert_match = [&](size_t idx) {
                if(idx >= Size || Charset::size == 0) return;
                std::uniform_int_distribution<size_t> idx_dist(0, Charset::size - 1);
                data[idx] = Charset::chars[idx_dist(gen)];
            };

            if constexpr (M == Mode::MatchLast) insert_match(Size - 1);
            else if constexpr (M == Mode::MatchMiddle) insert_match(Size / 2);
            else if constexpr (M == Mode::MatchFirst1P) insert_match(Size / 100);
        }

        return result;
    }();
    return pair;
}

// ============================================================================
// Benchmark Runners
// ============================================================================

// 原有版本
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

// 预设字符集版本
template <size_t Size, Mode M, Preset P>
void BM_run_preset(benchmark::State &state, auto &&func) {
    const auto& [data, byteset] = generate_data_preset<Size, M, P>();

    for(auto _ : state) {
        auto res = func(data, byteset);
        benchmark::DoNotOptimize(res);
        benchmark::DoNotOptimize(data.data());
    }

    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(Size));
}

// 概率版本
template <size_t Size, Prob P>
void BM_run_prob(benchmark::State &state, auto &&func) {
    const auto& [data, byteset] = generate_data_prob<Size, P>();

    for(auto _ : state) {
        auto res = func(data, byteset);
        benchmark::DoNotOptimize(res);
        benchmark::DoNotOptimize(data.data());
    }

    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(Size));
}

// 字符范围版本
template <size_t Size, Mode M, CharRange R>
void BM_run_ranged(benchmark::State &state, auto &&func) {
    const auto& [data, byteset] = generate_data_ranged<Size, M, R>();

    for(auto _ : state) {
        auto res = func(data, byteset);
        benchmark::DoNotOptimize(res);
        benchmark::DoNotOptimize(data.data());
    }

    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(Size));
}

// 字符集大小版本
template <size_t Size, Mode M, SetSize S>
void BM_run_sized(benchmark::State &state, auto &&func) {
    const auto& [data, byteset] = generate_data_sized<Size, M, S>();

    for(auto _ : state) {
        auto res = func(data, byteset);
        benchmark::DoNotOptimize(res);
        benchmark::DoNotOptimize(data.data());
    }

    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(Size));
}

// 编译时字符集版本 (测试 if chain)
template <size_t Size, Mode M, typename Charset>
void BM_run_static_ct(benchmark::State &state) {
    const auto& [data, byteset] = generate_data_for_static_charset<Size, M, Charset>();

    for(auto _ : state) {
        auto res = find_chars_static_ct<Charset>(data);
        benchmark::DoNotOptimize(res);
        benchmark::DoNotOptimize(data.data());
    }

    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(Size));
}

// 编译时字符集生成 bitmap 版本
template <size_t Size, Mode M, typename Charset>
void BM_run_bitmap_ct(benchmark::State &state) {
    const auto& [data, byteset] = generate_data_for_static_charset<Size, M, Charset>();

    for(auto _ : state) {
        auto res = find_chars_bitmap_ct<Charset>(data);
        benchmark::DoNotOptimize(res);
        benchmark::DoNotOptimize(data.data());
    }

    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(Size));
}

// 运行时小字符集循环版本
template <size_t Size, Mode M, typename Charset>
void BM_run_loop_rt(benchmark::State &state) {
    const auto& [data, byteset] = generate_data_for_static_charset<Size, M, Charset>();
    
    // 运行时复制字符数组 (防止编译器过度优化)
    auto chars_copy = Charset::chars;
    benchmark::DoNotOptimize(chars_copy.data());

    for(auto _ : state) {
        auto res = find_chars_loop_rt(data, chars_copy);
        benchmark::DoNotOptimize(res);
        benchmark::DoNotOptimize(data.data());
    }

    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(Size));
}

// ============================================================================
// 注册函数
// ============================================================================

// 原有模式注册
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

// 预设字符集注册
template <Mode M, Preset P, size_t ...Is>
void register_preset_tests(std::integer_sequence<size_t, Is...>,
                           std::string alg_name, auto func) {
    (benchmark::RegisterBenchmark(
        (alg_name + "/preset/" + preset_name(P) + "/" + mode_name(M) + "/" + 
         std::to_string(Is)).c_str(),
        [func](benchmark::State& state) {
            BM_run_preset<Is, M, P>(state, func);
        }
    ), ...);
}

// 概率测试注册
template <Prob P, size_t ...Is>
void register_prob_tests(std::integer_sequence<size_t, Is...>,
                         std::string alg_name, auto func) {
    (benchmark::RegisterBenchmark(
        (alg_name + "/prob/" + prob_name(P) + "/" + std::to_string(Is)).c_str(),
        [func](benchmark::State& state) {
            BM_run_prob<Is, P>(state, func);
        }
    ), ...);
}

// 字符范围注册
template <Mode M, CharRange R, size_t ...Is>
void register_range_tests(std::integer_sequence<size_t, Is...>,
                          std::string alg_name, auto func) {
    (benchmark::RegisterBenchmark(
        (alg_name + "/range/" + range_name(R) + "/" + mode_name(M) + "/" + 
         std::to_string(Is)).c_str(),
        [func](benchmark::State& state) {
            BM_run_ranged<Is, M, R>(state, func);
        }
    ), ...);
}

// 字符集大小注册
template <Mode M, SetSize S, size_t ...Is>
void register_sized_tests(std::integer_sequence<size_t, Is...>,
                          std::string alg_name, auto func) {
    (benchmark::RegisterBenchmark(
        (alg_name + "/sized/" + setsize_name(S) + "/" + mode_name(M) + "/" + 
         std::to_string(Is)).c_str(),
        [func](benchmark::State& state) {
            BM_run_sized<Is, M, S>(state, func);
        }
    ), ...);
}

// 编译时字符集注册 (if chain 版本)
template <Mode M, typename Charset, size_t ...Is>
void register_static_ct_tests(std::integer_sequence<size_t, Is...>,
                              std::string charset_name) {
    (benchmark::RegisterBenchmark(
        ("scalar_static_ct/" + charset_name + "/" + mode_name(M) + "/" + 
         std::to_string(Is)).c_str(),
        [](benchmark::State& state) {
            BM_run_static_ct<Is, M, Charset>(state);
        }
    ), ...);
}

// 编译时 bitmap 版本注册
template <Mode M, typename Charset, size_t ...Is>
void register_bitmap_ct_tests(std::integer_sequence<size_t, Is...>,
                              std::string charset_name) {
    (benchmark::RegisterBenchmark(
        ("scalar_bitmap_ct/" + charset_name + "/" + mode_name(M) + "/" + 
         std::to_string(Is)).c_str(),
        [](benchmark::State& state) {
            BM_run_bitmap_ct<Is, M, Charset>(state);
        }
    ), ...);
}

// 运行时循环版本注册
template <Mode M, typename Charset, size_t ...Is>
void register_loop_rt_tests(std::integer_sequence<size_t, Is...>,
                            std::string charset_name) {
    (benchmark::RegisterBenchmark(
        ("scalar_loop_rt/" + charset_name + "/" + mode_name(M) + "/" + 
         std::to_string(Is)).c_str(),
        [](benchmark::State& state) {
            BM_run_loop_rt<Is, M, Charset>(state);
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

    // ========================================
    // 基本算法 lambda
    // ========================================
    auto avx2_fn = [](auto &&rng, const auto &set) { 
        return find_byteset_avx2(rng, set); 
    };

    auto scalar_fn = [](auto &&rng, const auto &set) { 
        return find_byteset_scalar(rng, set); 
    };

    auto std_fn = [](auto &&rng, const auto &set) {
        auto it = std::ranges::find_if(rng, [&](char c) {
            auto uc = static_cast<unsigned char>(c);
            return (set[uc / 8] >> (uc % 8)) & 1;
        });
        if(it == std::ranges::end(rng)) return (ssize_t)-1;
        return (ssize_t)std::distance(std::ranges::begin(rng), it);
    };

    // ========================================
    // 1. 原有测试 (保持兼容)
    // ========================================
    auto register_all_modes = [&](std::string name, auto func) {
        register_tests_for_mode<Mode::NoMatch>(seq, name, func);
        register_tests_for_mode<Mode::MatchLast>(seq, name, func);
        register_tests_for_mode<Mode::MatchMiddle>(seq, name, func);
        register_tests_for_mode<Mode::MatchFirst1P>(seq, name, func);
        register_tests_for_mode<Mode::Random>(seq, name, func);
    };

    register_all_modes("avx2", avx2_fn);
    register_all_modes("scalar", scalar_fn);
    register_all_modes("std", std_fn);

    // ========================================
    // 2. 预设字符集测试 (JSON, HTML, Whitespace 等)
    // ========================================
    auto register_all_presets = [&](std::string name, auto func) {
        // NoMatch 模式 - 测吞吐量
        register_preset_tests<Mode::NoMatch, Preset::Json>(seq, name, func);
        register_preset_tests<Mode::NoMatch, Preset::Html>(seq, name, func);
        register_preset_tests<Mode::NoMatch, Preset::Whitespace>(seq, name, func);
        register_preset_tests<Mode::NoMatch, Preset::LineBreak>(seq, name, func);
        register_preset_tests<Mode::NoMatch, Preset::Digits>(seq, name, func);
        
        // MatchMiddle 模式 - 测典型场景
        register_preset_tests<Mode::MatchMiddle, Preset::Json>(seq, name, func);
        register_preset_tests<Mode::MatchMiddle, Preset::Html>(seq, name, func);
        register_preset_tests<Mode::MatchMiddle, Preset::Whitespace>(seq, name, func);
    };

    register_all_presets("avx2", avx2_fn);
    register_all_presets("scalar", scalar_fn);

    // ========================================
    // 3. 概率测试
    // ========================================
    auto register_all_probs = [&](std::string name, auto func) {
        register_prob_tests<Prob::P0_001>(seq, name, func);
        register_prob_tests<Prob::P0_01>(seq, name, func);
        register_prob_tests<Prob::P0_1>(seq, name, func);
        register_prob_tests<Prob::P1>(seq, name, func);
        register_prob_tests<Prob::P10>(seq, name, func);
        register_prob_tests<Prob::P50>(seq, name, func);
    };

    register_all_probs("avx2", avx2_fn);
    register_all_probs("scalar", scalar_fn);

    // ========================================
    // 4. 字符范围测试 (ASCII, High ASCII, Low64 等)
    // ========================================
    auto register_all_ranges = [&](std::string name, auto func) {
        register_range_tests<Mode::NoMatch, CharRange::Ascii>(seq, name, func);
        register_range_tests<Mode::NoMatch, CharRange::HighAscii>(seq, name, func);
        register_range_tests<Mode::NoMatch, CharRange::Low64>(seq, name, func);
        register_range_tests<Mode::NoMatch, CharRange::Printable>(seq, name, func);
    };

    register_all_ranges("avx2", avx2_fn);
    register_all_ranges("scalar", scalar_fn);

    // ========================================
    // 5. 字符集大小测试 (1, 2, 4, 8, 16, 32 个字符)
    // ========================================
    auto register_all_sizes = [&](std::string name, auto func) {
        register_sized_tests<Mode::NoMatch, SetSize::N1>(seq, name, func);
        register_sized_tests<Mode::NoMatch, SetSize::N2>(seq, name, func);
        register_sized_tests<Mode::NoMatch, SetSize::N4>(seq, name, func);
        register_sized_tests<Mode::NoMatch, SetSize::N8>(seq, name, func);
        register_sized_tests<Mode::NoMatch, SetSize::N16>(seq, name, func);
        register_sized_tests<Mode::NoMatch, SetSize::N32>(seq, name, func);
        
        register_sized_tests<Mode::MatchMiddle, SetSize::N1>(seq, name, func);
        register_sized_tests<Mode::MatchMiddle, SetSize::N4>(seq, name, func);
        register_sized_tests<Mode::MatchMiddle, SetSize::N8>(seq, name, func);
    };

    register_all_sizes("avx2", avx2_fn);
    register_all_sizes("scalar", scalar_fn);

    // ========================================
    // 6. 编译时字符集测试 - if chain 版本
    //    测试编译器对 if(ch == 'a' || ch == 'b' || ...) 的优化
    // ========================================
    // NoMatch 模式
    register_static_ct_tests<Mode::NoMatch, JsonCharsStatic>(seq, "json");
    register_static_ct_tests<Mode::NoMatch, HtmlCharsStatic>(seq, "html");
    register_static_ct_tests<Mode::NoMatch, WsCharsStatic>(seq, "ws");
    register_static_ct_tests<Mode::NoMatch, LfCharsStatic>(seq, "lf");
    register_static_ct_tests<Mode::NoMatch, DigitCharsStatic>(seq, "digit");
    
    // MatchMiddle 模式
    register_static_ct_tests<Mode::MatchMiddle, JsonCharsStatic>(seq, "json");
    register_static_ct_tests<Mode::MatchMiddle, HtmlCharsStatic>(seq, "html");
    register_static_ct_tests<Mode::MatchMiddle, WsCharsStatic>(seq, "ws");
    register_static_ct_tests<Mode::MatchMiddle, LfCharsStatic>(seq, "lf");

    // ========================================
    // 7. 编译时 bitmap 版本 (字符集编译时确定，bitmap 也编译时生成)
    // ========================================
    register_bitmap_ct_tests<Mode::NoMatch, JsonCharsStatic>(seq, "json");
    register_bitmap_ct_tests<Mode::NoMatch, HtmlCharsStatic>(seq, "html");
    register_bitmap_ct_tests<Mode::NoMatch, WsCharsStatic>(seq, "ws");
    register_bitmap_ct_tests<Mode::NoMatch, LfCharsStatic>(seq, "lf");
    
    register_bitmap_ct_tests<Mode::MatchMiddle, JsonCharsStatic>(seq, "json");
    register_bitmap_ct_tests<Mode::MatchMiddle, WsCharsStatic>(seq, "ws");

    // ========================================
    // 8. 运行时小字符集循环版本 (模拟运行时才知道字符集)
    // ========================================
    register_loop_rt_tests<Mode::NoMatch, JsonCharsStatic>(seq, "json");
    register_loop_rt_tests<Mode::NoMatch, HtmlCharsStatic>(seq, "html");
    register_loop_rt_tests<Mode::NoMatch, WsCharsStatic>(seq, "ws");
    register_loop_rt_tests<Mode::NoMatch, LfCharsStatic>(seq, "lf");
    
    register_loop_rt_tests<Mode::MatchMiddle, JsonCharsStatic>(seq, "json");
    register_loop_rt_tests<Mode::MatchMiddle, WsCharsStatic>(seq, "ws");

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}