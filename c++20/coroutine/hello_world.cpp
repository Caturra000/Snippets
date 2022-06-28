#include <coroutine>
#include <cstddef>
#include <iostream>

struct Promise;

struct Coroutine: public std::coroutine_handle<Promise> {
    using promise_type = Promise;
};

struct Promise {
    Coroutine get_return_object();
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
};

inline Coroutine Promise::get_return_object() {
    return { Coroutine::from_promise(*this) };
}

int main() {
    Coroutine co = [](int i) -> Coroutine {
        std::cout << i << std::endl;
        co_return;
    }(5);
    co.resume();
    co.destroy();
}