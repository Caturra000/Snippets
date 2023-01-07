#include <iostream>

struct O {
    ~O() { std::cout << "~O()\n"; }
};

struct wrapper {
    O const& val;
    ~wrapper() { std::cout << "~wrapper()\n"; }
};

// with explicit ctor
struct wrapperEx {
    O const& val;
    explicit wrapperEx(O const& val): val(val) {}
    ~wrapperEx() { std::cout << "~wrapperEx()\n"; }
};

template<class T>
T&& f(T&& t) {
    return std::forward<T>(t);
}

int main()
{
    std::cout << "case 1-----------\n";
    {
        // end-scope
        // ~wrapper()
        // ~O()
        auto&& a = wrapper{O()};
        std::cout << "end-scope\n";
    }
    std::cout << "case 2-----------\n";
    {
        // end-scope
        // ~O()
        // ~wrapper()
        auto a = wrapper{O()};
        std::cout << "end-scope\n";
    }
    std::cout << "case 3-----------\n";
    {
        // ~O()
        // end-scope
        // ~wrapper()
        auto&& a = wrapper{f(O())};
        std::cout << "end-scope\n";
    }
    std::cout << "case Ex-----------\n";
    {
        // ~O()
        // end-scope
        // ~wrapperEx()
        auto&& a = wrapperEx{O()};
        std::cout << "end-scope\n";
    }
    return 0;
}
