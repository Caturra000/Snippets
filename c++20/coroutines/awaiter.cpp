#include <coroutine>
#include <iostream>
#include <ranges>

// 文件基于generator.cpp修改
// commit: 8de26196a27ac3619f79e98c1d8765bedf3a7198
// * 添加了Awaiter类型
// * yield_value改为返回Awaiter对象


//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
struct Awaiter {
    bool await_ready() {
        std::cout << "ready?" << std::endl;
        return false;
    }
    auto await_suspend(std::coroutine_handle<> handle) {
        std::cout << "suspend!" << std::endl;
        std::cout << handle.address() << std::endl;
        return void();
        // return false;
        // return true;
        // return /*next_handle*/;
    }
    void await_resume() {
        std::cout << "resume!" << std::endl;
    }
};
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

template <typename T>
class My_generator {
public:
    struct promise_type;

    // ctor / dtor
    explicit constexpr
      My_generator(std::coroutine_handle<promise_type> h) noexcept
        : _handle(h) {}
    ~My_generator() { _handle ? _handle.destroy() : void(0); }

    // copy / move
    My_generator(const My_generator &) = delete;
    My_generator(My_generator &&rhs) = delete;
    My_generator& operator=(const My_generator &rhs) = delete;

    // user APIs
    T operator ()() {
        if(!resumable()) {
            throw std::runtime_error("AIEEEEE! A NINJA!?");
        }
        if(!operator bool()) {
            throw std::logic_error("WHY THERE'S A NINJA HERE!?");
        }
        _handle.resume();
        _handle.promise().count--;
        return std::move(_handle.promise().result);
    }

    explicit operator bool() const noexcept {
        return _handle.promise().count > 0;
    }

    bool resumable() const noexcept {
        return _handle && !_handle.done();
    }

private:
    std::coroutine_handle<promise_type> _handle;
};

template <typename T>
struct My_generator<T>::promise_type {
#ifdef __INTELLISENSE__
    // make IntelliSense happy
    promise_type() = default;
#endif

    explicit constexpr
      promise_type(size_t count) noexcept(noexcept(T())): count(count) {}
    auto initial_suspend() noexcept { return std::suspend_always{}; }
    auto final_suspend() noexcept { return std::suspend_never{}; }
    void unhandled_exception() {}

    My_generator<T> get_return_object();
    auto yield_value(auto &&value);

    size_t count;
    T result;
};

template <typename T>
My_generator<T> My_generator<T>::promise_type::get_return_object() {
    auto handle = std::coroutine_handle<promise_type>::from_promise(*this);
    return My_generator{handle};
}

template <typename T>
auto My_generator<T>::promise_type::yield_value(auto &&value) {
    result = std::forward<decltype(value)>(value);
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    return Awaiter{};
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
}

// C++ coroutines are special functions
static My_generator<int> coroutine_function(size_t n) {
    for(auto i : std::views::iota(0)) {
        co_yield i;
    }
}

int main() {
    constexpr size_t n = 7;
    auto generate = coroutine_function(n);
    while(generate) {
        std::cout << generate() << ' ';
    }
    return 0;
}
