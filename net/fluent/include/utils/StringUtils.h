#ifndef __FLUENT_UTILS_STRING_UTILS_H__
#define __FLUENT_UTILS_STRING_UTILS_H__
#include <bits/stdc++.h>
namespace fluent {

std::vector<std::pair<int, int>>
inline split(const char *str, const char pivot = ' ') {
    std::vector<std::pair<int, int>> result;
    int lo = 0, hi = 0;
    while(*str) {
        while(*str && *str == pivot) ++str, ++lo, ++hi;
        while(*str && *str != pivot) ++str, ++hi;
        if(lo < hi) result.emplace_back(lo, hi);
        lo = hi;
    }
    return result;
}

std::vector<std::pair<int, int>>
inline split(const char *str, int length, const char pivot = ' ') {
    std::vector<std::pair<int, int>> result;
    int lo = 0, hi = 0;
    while(length) {
        while(length && *str == pivot) ++str, ++lo, ++hi, --length;
        while(length && *str != pivot) ++str, ++hi, --length;
        if(lo < hi) result.emplace_back(lo, hi);
        lo = hi;
    }
    return result;
}

std::vector<std::pair<int, int>>
inline split(const std::string &str, const char pivot = ' ') {
    return split(str.c_str(), str.length(), pivot);
}

template<typename T>
inline T toDec(const std::string &str) {
    static_assert(std::is_integral<T>::value, "T must be integral");
    T dec = 0;
    for(auto c : str) {
        dec = dec*10 + (c - '0');
    }
    return dec;
}

} // fluent

#endif