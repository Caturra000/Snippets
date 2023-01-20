#pragma once
#include <bits/stdc++.h>

// 48bit pointer
// 只适用于x86_64四级分页的情况
// 使用高16bit作为tag避免ABA问题
// 如无特殊声明，tag是随机数
template <typename T>
struct Tagged_ptr {
    Tagged_ptr() = default;
    // Tagged_ptr(T *p) { std::memcpy(x, &p, 8); }
    Tagged_ptr(T *p, uint16_t t = rand()) { std::memcpy(x, &p, 6); x[3] = t;  }
    uint16_t get_tag() { return x[3]; }
    T* get_ptr() { return reinterpret_cast<T*>(to_val() & MASK);}
    T* operator->() { return get_ptr(); }
    T& operator*() { return *get_ptr(); }
    operator bool() { return !!get_ptr(); }
    uint64_t to_val() { return *reinterpret_cast<uint64_t*>(x); }
    // compared with tag
    bool operator==(Tagged_ptr p) { return to_val() == p.to_val(); }
    bool operator!=(Tagged_ptr p) { return !operator==(p);}
    constexpr static size_t MASK = (1ull<<48)-1;
    static uint16_t rand() {
        using std::chrono::steady_clock;
        static thread_local size_t good = 1926'08'17 ^ steady_clock::now().time_since_epoch().count();
        return good = (good * 998244353) + 12345;
    }
private:
    // low-48-bit: pointer
    // high-16-bit: tag
    uint16_t x[4];
};
