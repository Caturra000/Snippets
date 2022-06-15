#ifndef __FLUENT_UTILS_ALGORITHMS_H__
#define __FLUENT_UTILS_ALGORITHMS_H__
namespace fluent {

auto roundToPowerOfTwo = [](int v)->int {
    v--;
    v |= v >> 1; v |= v >> 2;
    v |= v >> 4; v |= v >> 8;
    v |= v >> 16;
    return ++v;
};

auto lowbit = [](int v) { return v&-v; };
auto isPowerOfTwo = [](int v) { return v == lowbit(v); };
auto highestBitPosition = [](int v) {
    for(int i = 31; ~i; --i) {
        if(v>>i&1) return i;
    }
    return 0;
};

auto ceilOfPowerOfTwo = [](int v) {
    int n = highestBitPosition(v);
    if(isPowerOfTwo(v)) return n;
    return n+1;
};

auto floorOfPowerOfTwo = [](int v) {
    int n = highestBitPosition(v);
    if(isPowerOfTwo(v)) return n;
    return n-1;
};

template <size_t N>
inline constexpr size_t hash(const char (&buf)[N]) {
    size_t primes[] = {19260817, 233, 998244353};
    size_t h = primes[0];
    for(size_t i = 0; i < N; ++i) {
        h += (buf[i]+primes[1]) * h + primes[2];
    }
    return h;
}

template <typename T = size_t, size_t init = hash(__TIME__)>
inline T random() {
    static thread_local auto seed = init;
    seed = seed * 998244353 + 12345;
    return static_cast<T>(seed / 1024);
}

} // fluent
#endif