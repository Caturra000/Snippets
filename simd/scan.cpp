#include <x86intrin.h>
#include <algorithm>
#include <ranges>
/// For test.
#include <array>
#include <cassert>
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <tuple>

template <auto First, auto Last>
constexpr auto constexpr_for = [](auto &&f) {
    static_assert(Last - First >= 0);
    return [&]<auto I>(this auto &&self) {
        if constexpr (I < Last) {
            f.template operator()<I>();
            return self.template operator()<I + 1>();
        } else {
            return Last - First;
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

template <size_t ILP = 4>
int scan_ilp(std::ranges::range auto &&rng) {
    constexpr auto lane = sizeof(__m256i) / sizeof(int);
    constexpr auto bulk = ILP * lane;
    auto inner_scan = [](const __m256i &v1) {
        // X2:
        // h g f e | d c b a (from v)
        // g f e 0 | c b a 0 (from s)
        // =>
        //  ...    | c+d,b+c,a+b,a
        auto s1 = _mm256_slli_si256(v1, sizeof(int));
        auto v2 = _mm256_add_epi32(v1, s1);

        // X4:
        //  ...    | c+d,b+c,a+b,a
        //  ...    | a+b,a
        // =>
        //  ...    | a+b+c+d,a+b+c,a+b,a
        auto s2 = _mm256_slli_si256(v2, 2 * sizeof(int));
        auto v3 = _mm256_add_epi32(v2, s2);

        // X8:
        // Cross lane:
        // a+b+c+d,a+b+c,a+b+c+d,a+b+c | a+b+c+d,a+b+c,a+b+c+d,a+b+c
        auto p3 = _mm256_permute4x64_epi64(v3, _MM_SHUFFLE(1, 1, 1, 1));
        // Broadcast:
        // a+b+c+d,a+b+c+d,a+b+c+d,a+b+c+d | a+b+c+d,a+b+c+d,a+b+c+d,a+b+c+d
        auto s3 = _mm256_shuffle_epi32(p3, _MM_SHUFFLE(3, 3, 3, 3));
        // a+b+c+d+e+f+g+h,a+b+c+d+e+f+g,a+b+c+d+e+f,a+b+c+d+e | XXXX
        auto a3 = _mm256_add_epi32(v3, s3);

        // What we need.
        return _mm256_blend_epi32(v3, a3, 0b11110000);
    };
    auto get_carry = [](const __m256i &result) {
        auto buffer = _mm256_permute4x64_epi64(result, _MM_SHUFFLE(3, 3, 3, 3));
        return _mm256_shuffle_epi32(buffer, _MM_SHUFFLE(1, 1, 1, 1));
    };
    auto sum = _mm256_setzero_si256();

    auto process_simd = [&](auto static_for, auto simd_view) {
        constexpr auto size = static_for([]<auto>{});
        __m256i results[size] {};
        __m256i carries[size] {};
        for(auto &&simd_v : simd_view) {
            static_for([&, addr = &simd_v]<auto Index> {
                auto loadu = _mm256_loadu_si256((__m256i*)(addr + Index * lane));
                results[Index] = inner_scan(loadu);
            });
            static_for([&]<auto Index> {
                carries[Index] = get_carry(results[Index]);
            });
            static_for([&, addr = &simd_v]<auto Index> {
                auto result = _mm256_add_epi32(results[Index], sum);
                _mm256_storeu_si256((__m256i*)(addr + Index * lane), result);
                sum = _mm256_add_epi32(sum, carries[Index]);
            });
        }
    };

    auto bulk_simd_view = rng
                        | simdify<lane>
                        | simdify<ILP>;
    process_simd(constexpr_for<0, ILP>, bulk_simd_view);

    auto single_simd_view = rng
                            | std::views::drop(bulk * std::ranges::size(bulk_simd_view))
                            | simdify<lane>;
    process_simd(constexpr_for<0, 1>, single_simd_view);

    auto scalar_view = rng
                     | std::views::drop(bulk * std::ranges::size(bulk_simd_view))
                     | std::views::drop(lane * std::ranges::size(single_simd_view));
    std::inclusive_scan(std::ranges::begin(scalar_view),
                        std::ranges::end(scalar_view),
                        std::ranges::begin(scalar_view),
                        std::plus(),
                        _mm256_cvtsi256_si32(sum));
    return 0;
}

//////////////////////////////////////////////////////////// For test.

auto initiate(auto &arr) {
    std::default_random_engine generator(std::random_device{}());
    std::uniform_int_distribution<int> distribution(0, 100); // no overflow
    for(auto &v : arr) {
        v = distribution(generator);
    }
    auto result = std::ranges::fold_left(arr, 0, std::plus());
    constexpr auto cacheline_elements = 64 / sizeof(arr[0]);
    auto flush_view = std::views::iota(arr.data())
        | std::views::stride(cacheline_elements)
        | std::views::take(arr.size() / cacheline_elements);
    for(auto addr : flush_view) {
        _mm_clflush(addr);
    }
    return result;
}

auto tick(auto f) {
    namespace stdc = std::chrono;
    auto start = stdc::steady_clock::now();
    auto v = f();
    auto end = stdc::steady_clock::now();
    auto elapsed = stdc::duration_cast<stdc::milliseconds>(end - start);
    return std::tuple(v, elapsed);
}

////////////////////////////////////////////////////////////

int main() {
    for(auto size : std::views::iota(1, 10000)) {
        std::vector<int> test_data(size);
        auto check = initiate(test_data);
        auto test_data_copy = test_data;
        auto [v, elapsed] = tick([&] { return scan_ilp(test_data); });
        std::inclusive_scan(test_data_copy.begin(),
                            test_data_copy.end(),
                            test_data_copy.begin(),
                            std::plus(), 0);
        for(auto i = 0; i < test_data.size(); ++i) {
            assert(test_data[i] == test_data_copy[i]);
        }
    }
    return 0;
}
