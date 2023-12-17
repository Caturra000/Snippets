#include <coroutine>
#include <functional>
#include <optional>
#include <exception>
#include <iostream>
#include <vector>
#include <type_traits>
#include <source_location>
#include <ranges>

// debug info
constexpr bool debug = true;
constexpr const char debug_prefix[] = "\033[34;1m";
constexpr const char debug_suffix[] = "\033[0m";

// customization points
constexpr bool initial_suspend_never = false;
constexpr bool final_suspend_never = true;
constexpr bool yield_value_suspend_never = false;

// helper function for CPs
template <auto never>
constexpr auto suspend_never_if() {
    if constexpr (never) {
        return std::suspend_never{};
    } else {
        return std::suspend_always{};
    }
}

template <typename ...Args>
void print_debug(Args &&...args) {
    if constexpr (debug) {
        // cannot #include <format>
        using std::cout;
        auto print = []<typename T>(T &&info) {
            cout << info << ' ';
        };
        cout << debug_prefix;
        (print(args), ...);
        cout << debug_suffix << std::endl;
    }
}

template <typename T>
class generator {
public:
    struct promise_type;

    // ctor / dtor
    generator(std::coroutine_handle<promise_type> h)
        : _handle(h) {}
    ~generator() {
        if(_handle) _handle.destroy();
    }

    // copy / move
    generator(const generator &) = delete;
    generator(generator &&rhs): _handle(rhs._handle) {rhs._handle = nullptr; }
    generator& operator=(generator rhs) {
        std::swap(*this, rhs);
        return *this;
    }

    // user api
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
    auto initial_suspend() const;
    auto final_suspend() const noexcept;
    void unhandled_exception() {}

    generator<T> get_return_object();

    // `co_yield` will pass `arg` to yield_value(arg)
    auto yield_value(T arg);

    // temporary storage
    std::optional<T> result;
};

template <typename T>
auto generator<T>::promise_type::initial_suspend() const {
    return suspend_never_if<initial_suspend_never>();
}

template <typename T>
auto generator<T>::promise_type::final_suspend() const noexcept {
    return suspend_never_if<final_suspend_never>();
};

template <typename T>
generator<T> generator<T>::promise_type::get_return_object() {
    print_debug(std::source_location::current().function_name());

    using this_type = std::decay_t<decltype(*this)>;
    auto handle = std::coroutine_handle<this_type>::from_promise(*this);
    // `handle` is just a pointer
    return generator<T>{handle};
}

template <typename T>
auto generator<T>::promise_type::yield_value(T arg) {
    print_debug("yield value:", arg);

    // make it different
    // if constexpr (std::is_integral_v<T>) {
    //     arg *= 2;
    // }

    result.emplace(std::move(arg));

    return suspend_never_if<yield_value_suspend_never>();
}

// C++ coroutines are special functions
// `range` is a C++ coroutine
static generator<int> range(int n) {
    for(int i = 0; i < n; ++i) {
        co_yield i;
    }
}

static generator<size_t> fib(size_t n) {
    constexpr size_t shift = 2;
    constexpr size_t mask = 3;
    size_t tab[2 + shift] = {0, 1};
    for(auto i : std::views::iota(shift, n + shift)) {
        tab[i & mask] = tab[i-1 & mask] + tab[i-2 & mask];
        co_yield tab[i & mask];
    }
    std::cin.get();
}

int main() {
    int n = 7;
    auto gen = fib(n);
    print_debug("get generator");

    for(auto i{0}, m{n}; m--; i++) {
        print_debug("===> before gen.next():", i);
        std::cout << gen.next() << std::endl;
        print_debug("<=== after gen.next():", i, '\n');
    }
    return 0;
}
