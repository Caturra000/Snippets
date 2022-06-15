#ifndef __FLUENT_UTILS_CALLABLE_H__
#define __FLUENT_UTILS_CALLABLE_H__
#include <bits/stdc++.h>
namespace fluent {

class Callable {
public:
    template <typename Func, typename ...Args>
    static Callable make(Func &&functor, Args &&...args) {
#if __cplusplus >= 201402L
        return Callable([=, f = std::forward<Func>(functor)]{f(args...);});
#else
        return Callable([=]{functor(args...);});
#endif
    }
    void call() const { _functor(); }
    void operator()() const { _functor(); }

public:
    Callable() : _functor([]{}) {}

protected:
    using Functor = std::function<void()>;
    Callable(const Functor &functor) : _functor(functor) {}
    Callable(Functor &&functor): _functor(static_cast<Functor&&>(functor)) {}
    Functor _functor;
};

} // fluent
#endif