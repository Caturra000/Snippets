#ifndef __FLUENT_UTILS_TYPE_TRAITS_H__
#define __FLUENT_UTILS_TYPE_TRAITS_H__
#include <functional>
namespace fluent {

// test:
// void f(int a) {}
// void g() {}
// auto p0 = isCallable(f);
// auto p1 = isCallable(f, 1);
// auto p2 = isCallable(g);
// auto p3 = isCallable(g, 1);
// auto p4 = isCallable([](std::string){}, "1");
//
// template <typename ...Args, typename = decltype(isCallable<Args...>)> void func();
template<typename F, typename ...Args>
inline constexpr auto isCallable(F &&f, Args &&...args)
        -> decltype(f(std::forward<Args>(args)...))* {
    return nullptr;
}

template<typename ...Args>
using IsCallableType = decltype(isCallable<Args...>);

} // fluent
#endif