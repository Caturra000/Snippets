#pragma once
#include <x86intrin.h>
#include <algorithm>
#include <ranges>
#include <cassert>

namespace stdr = std::ranges;
namespace stdv = std::views;

template <auto First, auto Last>
constexpr auto constexpr_for = [](auto &&f) {
    [&]<auto I>(this auto &&self) {
        if constexpr (I < Last) {
            f.template operator()<I>();
            self.template operator()<I + 1>();
        }
    }.template operator()<First>();
};

template <size_t Lane>
struct simdify_t: stdr::range_adaptor_closure<simdify_t<Lane>> {
    constexpr auto operator()(auto &&r) const noexcept {
        auto v = std::forward<decltype(r)>(r) | stdv::all;
        auto n = stdr::size(v) / Lane;
        auto i = stdv::iota(size_t{0}, n);
        auto f = [v](auto index) -> decltype(auto) {
            return v[index * Lane];
        };
        return stdv::transform(i, f);
    }
};
template <auto i>
constexpr simdify_t<i> simdify;

template <size_t ILP = 4>
void lookup_ilp(stdr::range auto &&source, stdr::range auto &&lookup_table) {
    assert(stdr::size(lookup_table) >= 256 && "We need a char-width table.");
    constexpr auto table_size = 256;
    constexpr auto lane = sizeof(__m256i);
    constexpr auto bulk = ILP * lane;

    __m256i luts[table_size / sizeof(__m128i)];
    constexpr_for<0, stdr::size(luts)>([&, data = stdr::data(lookup_table)]<auto Index> {
        auto buffer128 = _mm_loadu_si128((__m128i*)(data) + Index);
        luts[Index] = _mm256_broadcastsi128_si256(buffer128);
    });

    const auto simd_zero = _mm256_setzero_si256();
    auto reduce = [&](__m256i *simd_addr) {
        // Reused in each reduction.
        // The first round of reduction uses size(luts)/2.
        // The second round uses size(luts)/4, size(luts)/8, ...
        __m256i arenas[stdr::size(luts) / 2];
        auto full = _mm256_loadu_si256(simd_addr);
        auto nibble = _mm256_and_si256(full, _mm256_set1_epi8((char)0x0f));

        // Start from bit 4 to bit 7.
        constexpr auto bias = 4;
        constexpr auto for_each_bit = constexpr_for<4, 8>;
        for_each_bit([&]<auto Bit> {
            constexpr auto bit_shift = static_cast<char>(1 << Bit);
            auto test1 = _mm256_and_si256(full, _mm256_set1_epi8(bit_shift));
            auto blend_mask = _mm256_cmpeq_epi8(test1, simd_zero);

            // Bit 4 as the first round.
            constexpr auto round = Bit - bias;
            constexpr auto tree_depth = stdr::size(arenas) >> round;
            constexpr auto tree_reduce = constexpr_for<0, tree_depth>;
            tree_reduce([&]<auto Level> {
                auto emit = [&]<auto i> /* -> decltype(auto) */ {
                    if constexpr (round > 0) return arenas[i];
                    else return _mm256_shuffle_epi8(luts[i], nibble);
                };
                auto &&lo = emit.template operator()<Level * 2>();
                auto &&hi = emit.template operator()<Level * 2 + 1>();
                arenas[Level] = _mm256_blendv_epi8(hi, lo, blend_mask);
            });
        });
        _mm256_storeu_si256(simd_addr, arenas[0]);
    };

    auto process_simd = [&](auto konstexpr_for, auto simd_view) {
        for(auto &&simd_v : simd_view) {
            konstexpr_for([&, addr = &simd_v]<auto Index> {
                reduce((__m256i*)(addr) + Index);
            });
        }
    };

    auto bulk_simd_view = source | simdify<lane> | simdify<ILP>;
    process_simd(constexpr_for<0, ILP>, bulk_simd_view);

    auto single_simd_view = source
                          | stdv::drop(bulk * stdr::size(bulk_simd_view))
                          | simdify<lane>;
    process_simd(constexpr_for<0, 1>, single_simd_view);

    auto scalar_view = source
                     | stdv::drop(bulk * stdr::size(bulk_simd_view))
                     | stdv::drop(lane * stdr::size(single_simd_view));
    for(auto &v : scalar_view) {
        v = lookup_table[v];
    }
}

void lookup_avx2(stdr::range auto &&source, stdr::range auto &&lookup_table) {
    assert(stdr::size(lookup_table) >= 256 && "We need a char-width table.");
    constexpr auto table_size = 256;
    constexpr auto lane = sizeof(__m256i);
    __m256i luts[table_size / sizeof(__m128i)];
    constexpr_for<0, stdr::size(luts)>([&, data = stdr::data(lookup_table)]<auto Index> {
        auto buffer128 = _mm_loadu_si128((__m128i*)(data) + Index);
        luts[Index] = _mm256_broadcastsi128_si256(buffer128);
    });

    // Reused in each reduction.
    // The first round of reduction uses size(luts)/2.
    // The second round uses size(luts)/4, size(luts)/8, ...
    __m256i arenas[stdr::size(luts) / 2];

    auto simd_zero = _mm256_setzero_si256();
    auto simd_view = source | simdify<lane>;
    for(auto &&simd_v : simd_view) {
        auto addr = &simd_v;
        auto full = _mm256_loadu_si256((__m256i*)addr);
        auto nibble = _mm256_and_si256(full, _mm256_set1_epi8((char)0x0f));

        // Start from bit 4 to bit 7.
        constexpr auto bias = 4;
        constexpr auto for_each_bit = constexpr_for<4, 8>;
        for_each_bit([&]<auto Bit> {
            constexpr auto bit_shift = static_cast<char>(1 << Bit);
            auto test1 = _mm256_and_si256(full, _mm256_set1_epi8(bit_shift));
            auto blend_mask = _mm256_cmpeq_epi8(test1, simd_zero);

            // Bit 4 as the first round.
            constexpr auto round = Bit - bias;
            constexpr auto tree_depth = stdr::size(arenas) >> round;
            constexpr auto tree_reduce = constexpr_for<0, tree_depth>;
            tree_reduce([&]<auto Level> {
                auto emit = [&]<auto i> /* -> decltype(auto) */ {
                    if constexpr (round > 0) return arenas[i];
                    else return _mm256_shuffle_epi8(luts[i], nibble);
                };
                auto &&lo = emit.template operator()<Level * 2>();
                auto &&hi = emit.template operator()<Level * 2 + 1>();
                arenas[Level] = _mm256_blendv_epi8(hi, lo, blend_mask);
            });
        });
        _mm256_storeu_si256((__m256i*)addr, arenas[0]);
    }
    auto scalar_view = source
                     | stdv::drop(lane * stdr::size(simd_view));
    for(auto &v : scalar_view) {
        v = lookup_table[v];
    }
}

void lookup_scalar(auto &&rng, auto &&lut) {
    for(auto &v : rng) {
        v = lut[v];
    }
}
