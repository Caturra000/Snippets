#pragma once
#include <x86intrin.h>
#include <algorithm>
#include <ranges>
#include <iostream>
#include <bit>
#include <cassert>
#include <array>

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

int sum_avx2(std::ranges::range auto &&rng) {
    constexpr auto lane = sizeof(__m256i) / sizeof(int);
    __m256i partial_sum {};

    auto simd_view = rng
                   | simdify<lane>;
    for(auto &&simd_v : simd_view) {
        auto addr = &simd_v;
        auto loadu = _mm256_loadu_si256((__m256i*)addr);
        partial_sum = _mm256_add_epi32(partial_sum, loadu);
    }

    auto scalar_view = rng
                     | std::views::drop(lane * std::ranges::size(simd_view));
    int sum = std::ranges::fold_left(scalar_view, 0, std::plus());

    int temp[lane];
    _mm256_storeu_si256((__m256i*)std::ranges::data(temp), partial_sum);
    sum = std::ranges::fold_left(temp, sum, std::plus());

    return sum;
}

template <size_t ILP = 4>
int sum_avx2_ilp(std::ranges::range auto &&rng) {
    constexpr auto lane = sizeof(__m256i) / sizeof(int);
    constexpr auto bulk = lane * ILP;
    __m256i partial_sum[ILP] {};
    auto process_simd = [&](auto konstexpr_for, auto simd_view) {
        for(auto &&simd_v : simd_view) {
            konstexpr_for([&, addr = &simd_v]<auto Index> {
                auto &partial = partial_sum[Index];
                auto loadu = _mm256_loadu_si256(
                    (__m256i*)(addr + Index * lane));
                partial = _mm256_add_epi32(partial, loadu);
            });
        }
    };

    // SIMD+ILP
    auto bulk_simd_view = rng
                        | simdify<lane>
                        | simdify<ILP>;
    process_simd(constexpr_for<0, ILP>, bulk_simd_view);

    // SIMD
    auto single_simd_view = rng
                          | std::views::drop(bulk * std::ranges::size(bulk_simd_view))
                          | simdify<lane>;
    process_simd(constexpr_for<0, 1>, single_simd_view);

    auto scalar_view = rng
                     | std::views::drop(bulk * std::ranges::size(bulk_simd_view))
                     | std::views::drop(lane * std::ranges::size(single_simd_view));
    int sum = std::ranges::fold_left(scalar_view, 0, std::plus());

    constexpr_for<1, ILP>([&]<size_t Index> {
        partial_sum[0] = _mm256_add_epi32(partial_sum[0], partial_sum[Index]);
    });
    int temp[lane];
    _mm256_storeu_si256((__m256i*)std::ranges::data(temp), partial_sum[0]);
    sum = std::ranges::fold_left(temp, sum, std::plus());

    return sum;
}
