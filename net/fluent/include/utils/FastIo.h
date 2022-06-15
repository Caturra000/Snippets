#ifndef __FLUENT_UTILS_FAST_IO_H__
#define __FLUENT_UTILS_FAST_IO_H__
#include <bits/stdc++.h>
namespace fluent {

struct IoResult {
    const char *buf;
    size_t len;
    operator bool() { return buf != nullptr; }
};

template <size_t N>
class FastIo {
public:
    IoResult getline(std::istream &in = std::cin);

private:
    void clear();
    size_t strlen();
    size_t strlen2();

    static constexpr size_t M  = ((N+3 >> 2) << 3) + 8;
    char _buf[M] {};
    size_t cur = 0;
};

template <size_t N>
inline IoResult FastIo<N>::getline(std::istream &in) {
    if(cur > N) clear();
    if(!in.getline(_buf + cur, M - cur)) return {nullptr, 0};
    size_t bound = strlen2();
    IoResult result = {_buf + cur, bound - cur};
    cur = bound;
    return result;
}

template <size_t N>
inline void FastIo<N>::clear() {
    memset(_buf, 0, cur);
    cur = 0;
}

template <size_t N>
inline size_t FastIo<N>::strlen() {
    size_t lo = cur, hi = M-1;
    while(lo < hi) {
        size_t mid = lo + (hi-lo >> 1);
        if(_buf[mid] == '\0') hi = mid;
        else lo = mid+1;
    }
    return lo;
}

template <size_t N>
inline size_t FastIo<N>::strlen2() {
    size_t lo = cur >> 3;
    size_t hi = M-1 >> 3;
    while(lo < hi) {
        size_t mid = lo + (hi-lo >> 1);
        auto chars = *((long long*)(_buf + (mid << 3)));
        if((chars & 0xff) == 0) hi = mid;
        else lo = mid + 1;
    }
    lo = ((lo ? lo-1 : lo) << 3);
    for(int i = lo, j = lo + 8; i <= j; ++i) {
        if(_buf[i] == '\0') return i;
    }
    return M-1;
}

} // fluent
#endif