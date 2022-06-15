#ifndef __FLUENT_UTILS_POINTER_H__
#define __FLUENT_UTILS_POINTER_H__
#include <bits/stdc++.h>
namespace fluent {

// movable
template <typename T>
class Pointer {
public:
    constexpr Pointer(T* ptr = nullptr): _ptr(ptr) {}
    constexpr Pointer(const Pointer &rhs): _ptr(rhs._ptr) {}
    constexpr Pointer(Pointer &&rhs): _ptr(rhs._ptr) { rhs._ptr = nullptr; }
    constexpr Pointer operator++() { return ++_ptr; }
    constexpr Pointer operator++(int) { return _ptr++; }
    Pointer& operator=(Pointer rhs);
    T& operator*() { return *_ptr; }
    T* operator->() { return _ptr; }
    constexpr explicit operator bool() { return _ptr; }
    ~Pointer() {}

    template <typename Base> Base* castTo();
    T* get() { return _ptr; }

protected:
    T* _ptr;
};

template <typename T>
inline Pointer<T>& Pointer<T>::operator=(Pointer rhs) {
    std::swap(_ptr, rhs._ptr);
    return *this;
}

template <typename T>
template <typename Base>
inline Base* Pointer<T>::castTo() {
    static_assert(std::is_base_of<Base, T>::value, "can only cast to Pointer<Base>/Base*");
    return _ptr;
}

} // fluent

#endif