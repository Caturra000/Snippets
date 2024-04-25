#include <coroutine>
#include <functional>
#include <optional>
#include <stdexcept>
#include <iostream>

// Memleak option.
constexpr bool diff = false;

template <typename T>
class generator {
public:
    struct promise_type;

    generator(std::coroutine_handle<promise_type> h)
        : _handle(h) {}
    ~generator() {
        // NOTE!
        if constexpr (diff) {
            if(_handle) _handle.destroy();
        }
    }

    generator(const generator &) = delete;
    generator(generator &&rhs): _handle(rhs._handle) {rhs._handle = nullptr; }
    generator& operator=(generator rhs) {
        std::swap(*this, rhs);
        return *this;
    }

    T& next() {
        if(!_handle || _handle.done()) {
            throw std::runtime_error("check coroutine logic");
        }
        _handle.resume();
        return _handle.promise().result.value();
    }

private:
    std::coroutine_handle<promise_type> _handle;
};

template <typename T>
struct generator<T>::promise_type {
    auto initial_suspend() const { return std::suspend_always{}; };
    auto final_suspend() const noexcept { return std::suspend_never{}; }
    void unhandled_exception() {}

    generator<T> get_return_object();

    auto yield_value(T arg);

    std::optional<T> result;

    promise_type() {
        std::cout << "Hey!" << std::endl;
    }

    // NOTE!
    ~promise_type() {
        std::cout << "Bye!" << std::endl;
    }
};

template <typename T>
generator<T> generator<T>::promise_type::get_return_object() {
    using this_type = std::decay_t<decltype(*this)>;
    auto handle = std::coroutine_handle<this_type>::from_promise(*this);
    return generator<T>{handle};
}

template <typename T>
auto generator<T>::promise_type::yield_value(T arg) {
    result.emplace(std::move(arg));
    return std::suspend_always{};
}

static generator<int> make_generator(int n) {
    for(int i = 0; i < n; ++i) {
        co_yield i;
    }
}

int main() {
    int n = 7;
    auto gen = make_generator(n);
    // 即使一个未执行过的协程也需要考虑内存泄漏问题
    // 此时协程帧显然构造了（输出hey），但是没有析构（没有输出bye）
    return 0;
}
