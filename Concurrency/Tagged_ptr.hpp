#pragma once
#include <bits/stdc++.h>

// 48bit pointer
// 只适用于x86_64四级分页的情况
// 使用高16bit作为tag避免ABA问题
// 如无特殊声明，tag是随机数
template <typename T>
class Tagged_ptr {
private:
    union Cast_union {uint64_t x; uint16_t y[4];};

public:
    Tagged_ptr() = default;
    Tagged_ptr(T *p, uint16_t t = rand()): _ptr_tag(make(p, t)) {}
    uint16_t get_tag() { return _ptr_tag >> 48; }
    uint16_t next_tag() { return (get_tag() + 1) & TAG_MASK;}
    T* get_ptr() { return reinterpret_cast<T*>(_ptr_tag & PTR_MASK); }
    void set_ptr(T *p) { _ptr_tag = make(p, get_tag()); }
    void set_tag(uint16_t t) { _ptr_tag = make(get_ptr(), t); }

    T* operator->() { return get_ptr(); }
    T& operator*() { return *get_ptr(); }
    operator bool() { return !!get_ptr(); }
    bool operator==(T *p) { return make(get_ptr(), 0) == make(p, 0); }
    bool operator!=(T *p) { return !operator==(p); }
    // bool operator==(Tagged_ptr p) { return operator==(p.get_ptr()); }
    // bool operator!=(Tagged_ptr p) { return !operator==(p); }
    bool operator==(std::nullptr_t) { return !operator bool(); }
    bool operator!=(std::nullptr_t) { return operator bool(); }

private:
    constexpr static size_t PTR_MASK = (1ull<<48)-1;
    constexpr static size_t TAG_MASK = (1ull<<16)-1;
    static uint16_t rand() {
        using std::chrono::steady_clock;
        static thread_local size_t good = 1926'08'17 ^ steady_clock::now().time_since_epoch().count();
        return good = (good * 998244353) + 12345;
    }
    static uint64_t make(T *p, uint16_t t) {
        Cast_union cu;
        cu.x = uint64_t(p);
        cu.y[3] = t;
        return cu.x;
    }

private:
    uint64_t _ptr_tag;
};
