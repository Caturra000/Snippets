#pragma once
#include <x86intrin.h>
#include <algorithm>
#include <ranges>
#include <cstdint>

namespace stdv = std::views;
namespace stdr = std::ranges;

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

// For slow path.
inline constexpr struct escape_mask_predefined {
    // 2^^8 == 256
    inline constexpr static size_t Table = 256;
    inline constexpr static size_t Lane = 16;
    alignas(16) uint8_t for_shuffle[Table][Lane];
    alignas(16) uint8_t for_blend[Table][Lane];
    alignas(16) uint8_t lengths[Table];

    constexpr escape_mask_predefined() {
        for(auto mask : stdv::iota(0, 256)) {
            auto length = 0;
            for(auto index : stdv::iota(0, 8)) {
                // "\"
                if(mask >> index & 1) {
                    // Will be overwritten by blendv.
                    for_shuffle[mask][length] = 0x80;
                    // Marked as a backslash.
                    for_blend[mask][length] = 0xff;
                    length++;
                }
                for_shuffle[mask][length] = index;
                for_blend[mask][length] = 0;
                length++;
            }
            lengths[mask] = length;
            for(auto index : stdv::iota(length, 16)) {
                for_shuffle[mask][index] = 0x80;
                for_blend[mask][index] = 0;
            }
        }
    }
} escape_lut;

// Example:
// src: "Hello"world""
// dst: "Hello\"world\""
//
// dst should be larger than src, at least 2x.
// Return the final length of dst, this might be useful for null-based string.
size_t escape_avx2(std::ranges::range auto &&src, std::ranges::range auto &&dst) {
    const auto quote = _mm256_set1_epi8('"');
    const auto backslash = _mm256_set1_epi8('\\');
    const auto backslash128 = _mm_set1_epi8('\\');

    constexpr auto lane = sizeof(__m256i) / sizeof(char);
    size_t length = 0;

    auto escape_8x = [&](auto &&chunk128, uint8_t mask) {
        auto shuffle = _mm_load_si128((__m128i*) escape_lut.for_shuffle[mask]);
        auto blend = _mm_load_si128((__m128i*) escape_lut.for_blend[mask]);
        auto expanded = _mm_shuffle_epi8(chunk128, shuffle);
        auto result = _mm_blendv_epi8(expanded, backslash128, blend);
        _mm_storeu_si128((__m128i*)(stdr::data(dst) + length), result);
        return escape_lut.lengths[mask];
    };

    auto simd_view = src | simdify<lane>;
    for(auto &&simd_v : simd_view) {
        auto addr = (__m256i *) &simd_v;
        auto chunk = _mm256_loadu_si256(addr);
        auto mask = _mm256_movemask_epi8(_mm256_or_si256(
            _mm256_cmpeq_epi8(chunk, quote),
            _mm256_cmpeq_epi8(chunk, backslash)
        ));
        if(mask == 0) [[likely]] {
            _mm256_storeu_si256((__m256i *)(stdr::data(dst) + length), chunk);
            length += lane;
        } else {
            auto lo = _mm256_castsi256_si128(chunk);
            auto hi = _mm256_extracti128_si256(chunk, 1);
            length += escape_8x(lo,                    mask);
            length += escape_8x(_mm_srli_si128(lo, 8), mask >> 8);
            length += escape_8x(hi,                    mask >> 16);
            length += escape_8x(_mm_srli_si128(hi, 8), mask >> 24);
        }
    }
    auto scalar_view = src
                     | stdv::drop(lane * stdr::size(simd_view));
    for(auto v : scalar_view) {
        auto cond = (v == '\\' || v == '"');
        dst[length] = '\\';
        dst[length + cond] = v;
        length = length + cond + 1;
    }
    return length;
}
