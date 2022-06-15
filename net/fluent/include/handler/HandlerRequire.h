#ifndef __FLUENT_HANDLER_HANDLER_REQUIRE_H
#define __FLUENT_HANDLER_HANDLER_REQUIRE_H
#include <bits/stdc++.h>
namespace fluent {

template <typename Ret, typename ...Args>
struct HandlerRequire;

template <typename Ret, typename ...Args>
struct HandlerRequire<Ret(Args...)> {
    template <typename C>
    constexpr static void define(Ret (C::*fp)(Args...)) {}
};

} // fluent
#endif