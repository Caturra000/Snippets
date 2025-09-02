#pragma once
#include <cstdint>
#include <cstddef>

// 48bit pointer
// 只适用于x86_64四级分页的情况
// 使用高16bit作为tag避免ABA问题
// 如无特殊声明，tag是随机数
template <typename T>
class Tagged_ptr {
public:
    Tagged_ptr() = default;
    Tagged_ptr(T *p, uint16_t t = 0): _ptr_tag(make(p, t)) {}
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
    bool operator==(std::nullptr_t) { return !operator bool(); }
    bool operator!=(std::nullptr_t) { return operator bool(); }

private:
    constexpr static size_t PTR_MASK = (1ull<<48)-1;
    constexpr static size_t TAG_MASK = (1ull<<16)-1;

    static uint64_t make(T *p, uint16_t t) {
        union {uint64_t x; uint16_t y[4];};
        x = uint64_t(p);
        y[3] = t;
        return x;
    }

private:
    uint64_t _ptr_tag;
};
