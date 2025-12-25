#pragma once
#include <x86intrin.h>
#include <algorithm>
#include <ranges>
#include <climits>

template <size_t Lane>
struct simdify_t: std::ranges::range_adaptor_closure<simdify_t<Lane>> {
    constexpr auto operator()(auto &&r) const noexcept {
        auto v = std::forward<decltype(r)>(r) | std::views::all;
        auto n = std::ranges::size(v) / Lane;
        auto i = std::views::iota(size_t{0}, n);
        auto f = [v](auto index) -> decltype(auto) {
            return v[index * Lane];
        };
        return std::views::transform(i, f);
    }
};
template <auto i>
constexpr simdify_t<i> simdify;

// charset: u8[32] as a bitmap of char field.
//
// Core algorithm:
// u8[32] -> 32 x 8 matrix
//        -> 16 x 2 x 8
//        -> 16 x 8 (table1) + 16 x 8 (table2)
//        -> 16 x 8          + 16 x 8
//           ^^table-odd       ^^table-even
// b3-b7 (w5): byte index (row). We use b3 to select the table, which determines the parity.
// b0-b2 (w3): bit index (col).
ssize_t find_charset_avx2(std::ranges::range auto &&rng, const auto &charset) {
    // TODO: check u64[4]...
    static_assert(std::ranges::size(charset) == ((1 << CHAR_BIT) / CHAR_BIT));
    constexpr auto lane = sizeof(__m256i) / sizeof(char);

    const auto filter_lo = _mm_loadu_si128((__m128i *)(&charset));
    const auto filter_hi = _mm_loadu_si128((__m128i *)(&charset) + 1);
    const auto filter_mask = _mm_set1_epi16(0x00ff);
    const auto filter_lo_even = _mm_and_si128(filter_lo, filter_mask);
    const auto filter_hi_even = _mm_and_si128(filter_hi, filter_mask);
    const auto filter_lo_odd = _mm_srli_epi16(filter_lo, 8);
    const auto filter_hi_odd = _mm_srli_epi16(filter_hi, 8);
    // e4 e3 e2 e1
    const auto filter_even_128 = _mm_packus_epi16(filter_lo_even, filter_hi_even);
    // o4 o3 o2 o1
    const auto filter_odd_128 = _mm_packus_epi16(filter_lo_odd, filter_hi_odd);

    // e4 e3 e2 e1 | e4 e3 e2 e1
    const auto filter_even = _mm256_set_m128i(filter_even_128, filter_even_128);
    // o4 o3 o2 o1 | o4 o3 o2 o1
    const auto filter_odd = _mm256_set_m128i(filter_odd_128, filter_odd_128);

    // LUT for 1 << (b3b2b1b0 % 8). Equivalent to 1 << b2b1b0.
    const auto shift_mod = _mm256_setr_epi8(
        1, 2, 4, 8, 16, 32, 64, -128, // 1<<7 == -128
        1, 2, 4, 8, 16, 32, 64, -128,
        1, 2, 4, 8, 16, 32, 64, -128,
        1, 2, 4, 8, 16, 32, 64, -128
    );

    auto simd_view = rng | simdify<lane>;
    for(auto &&[index, simd_v] : std::views::enumerate(simd_view)) {
        auto addr = &simd_v;
        auto chars = _mm256_loadu_si256((__m256i*) addr);

        // b0-b3, conflict with b3
        auto bit_index = _mm256_and_si256(chars, _mm256_set1_epi8(0x0f));
        // b4-b7, conflict with b3
        auto byte_index = _mm256_and_si256(_mm256_srli_epi16(chars, 4), _mm256_set1_epi8(0x0f));

        // 1 << (n % 8). Actually we can set 0x07 and then calculate 1<<n.
        auto col = _mm256_shuffle_epi8(shift_mod, bit_index);

        auto row_even = _mm256_shuffle_epi8(filter_even, byte_index);
        auto row_odd  = _mm256_shuffle_epi8(filter_odd, byte_index);
        // Bit 7 (original bit 3) determines the table parity.
        auto parity = _mm256_slli_epi16(chars, 4);
        auto row = _mm256_blendv_epi8(row_even, row_odd, parity);

        auto movemask = _mm256_movemask_epi8(
            _mm256_cmpeq_epi8(_mm256_andnot_si256(row, col), _mm256_setzero_si256()));

        if(movemask) {
            return index * lane + std::countr_zero(unsigned(movemask));
        }
    }
    auto offset = lane * std::ranges::size(simd_view);
    auto scalar_view = rng | std::views::drop(offset);
    for(auto &&[index, _v] : std::views::enumerate(scalar_view)) {
        auto v = static_cast<unsigned char>(_v);
        auto v_row = charset[v / 8];
        auto v_col = 1 << (v % 8);
        if(v_row & v_col) return offset + index;
    }
    return -1;
}

struct avx2_ascii128_config {
    bool overflow;
};

// charset: u8[16] as a bitmap of char field.
//
// Core algorithm:
// u8[16] -> 16 x 8 matrix
//        -> 16 x 8
//           ^^table
// b3-b6 (w4): byte index (row).
// b0-b2 (w3): bit index (col).
template <avx2_ascii128_config Config>
ssize_t find_charset_avx2_ascii128(std::ranges::range auto &&rng, const auto &charset) {
    static_assert(std::ranges::size(charset) == ((1 << CHAR_BIT) / CHAR_BIT / 2));
    constexpr auto lane = sizeof(__m256i) / sizeof(char);
    auto filter128 = _mm_loadu_si128((__m128i *) &charset);
    auto filter256 = _mm256_set_m128i(filter128, filter128);

    // LUT for 1 << b2b1b0.
    const auto shift = _mm256_setr_epi8(
        1, 2, 4, 8, 16, 32, 64, -128,
        0, 0, 0, 0,  0,  0,  0,    0,
        1, 2, 4, 8, 16, 32, 64, -128,
        0, 0, 0, 0,  0,  0,  0,    0
    );

    auto simd_view = rng | simdify<lane>;
    for(auto &&[index, simd_v] : std::views::enumerate(simd_view)) {
        auto addr = (__m256i *) &simd_v;
        auto chars = _mm256_loadu_si256(addr);

        auto bit_index  = _mm256_and_si256(chars, _mm256_set1_epi8(0b00000111));
        auto byte_index = _mm256_and_si256(chars, _mm256_set1_epi8(0b01111000));
             byte_index = _mm256_srli_epi16(byte_index, 3);

        if constexpr (Config.overflow) {
            auto sign_bits = _mm256_and_si256(chars, _mm256_set1_epi8(0b10000000));
            byte_index = _mm256_or_si256(byte_index, sign_bits);
        }

        auto col = _mm256_shuffle_epi8(shift, bit_index);
        auto row = _mm256_shuffle_epi8(filter256, byte_index);

        auto movemask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(
            _mm256_and_si256(row, col), _mm256_setzero_si256()));
        if(unsigned counter = ~movemask) {
            return index * lane + std::countr_zero(counter);
        }
    }
    auto offset = lane * std::ranges::size(simd_view);
    auto scalar_view = rng | std::views::drop(offset);
    for(auto &&[index, _v] : std::views::enumerate(scalar_view)) {
        auto v = static_cast<unsigned char>(_v);
        if(v >= 128) continue;
        auto v_row = charset[v / 8];
        auto v_col = 1 << (v % 8);
        if(v_row & v_col) return offset + index;
    }
    return -1;
}

struct avx2_ascii128_transposed_config {
    bool transposed;
};

// charset: u8[16] as a bitmap of char field.
//
// Core algorithm:
// u8[16] -> 16 x 8 matrix
//        -> 16 x 8
//           ^^table
// b0-b3 (w4): byte index (row).
// b4-b6 (w3): bit index (col).
template <avx2_ascii128_transposed_config Config>
ssize_t find_charset_avx2_ascii128_transposed(std::ranges::range auto &&rng, const auto &_charset) {
    char charset[16];
    if constexpr (!Config.transposed) {
        std::ranges::fill(charset, 0);
        for(int c = 0; c < 128; c++) {
            if(_charset[c / 8] >> (c % 8) & 1) {
                charset[c % 16] |= 1 << (c / 16);
            }
        }
    } else {
        for(int i = 0; i < 16; ++i) charset[i] = _charset[i];
    }

    static_assert(std::ranges::size(charset) == ((1 << CHAR_BIT) / CHAR_BIT / 2));
    constexpr auto lane = sizeof(__m256i) / sizeof(char);
    auto filter128 = _mm_loadu_si128((__m128i *) &charset);
    auto filter256 = _mm256_set_m128i(filter128, filter128);

    // LUT for 1 << b7b6b5b4, but for high bit (ascii>=128, b7=1), set to 0.
    const auto shift_ignored = _mm256_setr_epi8(
        1, 2, 4, 8, 16, 32, 64, -128,
        0, 0, 0, 0,  0,  0,  0,    0,
        1, 2, 4, 8, 16, 32, 64, -128,
        0, 0, 0, 0,  0,  0,  0,    0
    );

    auto simd_view = rng | simdify<lane>;
    for(auto &&[index, simd_v] : std::views::enumerate(simd_view)) {
        auto addr = (__m256i *) &simd_v;
        auto chars = _mm256_loadu_si256(addr);

        auto &byte_index = chars; // We use lowbits.
        auto bit_index = _mm256_and_si256(_mm256_srli_epi16(chars, 4), _mm256_set1_epi8(0x0f));

        auto row = _mm256_shuffle_epi8(filter256, byte_index);
        auto col = _mm256_shuffle_epi8(shift_ignored, bit_index);

        auto movemask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(
            _mm256_and_si256(row, col), _mm256_setzero_si256()));
        if(unsigned counter = ~movemask) {
            return index * lane + std::countr_zero(counter);
        }
    }
    auto offset = lane * std::ranges::size(simd_view);
    auto scalar_view = rng | std::views::drop(offset);
    for(auto &&[index, _v] : std::views::enumerate(scalar_view)) {
        auto v = static_cast<unsigned char>(_v);
        if(v >= 128) continue;
        auto v_row = charset[v % 16];
        auto v_col = 1 << (v / 16);
        if(v_row & v_col) return offset + index;
    }
    return -1;
}

ssize_t find_charset_scalar(std::ranges::range auto &&rng, const auto &charset) {
    auto begin = std::ranges::begin(rng);
    auto end = std::ranges::end(rng);
    for(auto it = begin; it != end; ++it) {
        unsigned char c = static_cast<unsigned char>(*it);
        if((charset[c / 8] >> (c % 8)) & 1) {
            return std::distance(begin, it);
        }
    }
    return -1;
}
