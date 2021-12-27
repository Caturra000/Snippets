#include <bits/stdc++.h>

template <typename ...>
struct Function;

template <typename Ret, typename ...Args>
struct Function<Ret(Args...)> {

    struct Functor {
        virtual Ret operator()(Args &&...args) = 0;
        virtual ~Functor() = default;
    };

    template <typename C>
    struct FunctorImpl: Functor {
        FunctorImpl(C c): _callable(std::move(c)) {}
        Ret operator()(Args &&...args) override {
            return _callable(std::forward<Args>(args)...);
        }
        C _callable;
    };

    template <typename C>
    Function(C callable): _functor(new FunctorImpl<C>(std::move(callable))) {}
    ~Function() { delete _functor; }

    Ret operator()(Args &&...args) {
        return (*_functor)(std::forward<Args>(args)...);
    }

    Functor *_functor;
};

int main() {
    auto callable = [](int v, int factor) {
        return v * factor;
    };
    Function<int(int, int)> function(std::move(callable));
    std::cout << function(4, 3) << std::endl;
    return 0;
}