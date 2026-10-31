// 需要反汇编确认，使用 gcc-16 和 clang-23，均为 -O3 -std=c++23

struct X {
    X();
    ~X() {}
};

// 存在 guard，存在异常处理路径
X& x_normal_static() {
    static X data;
    return data;
}

struct Y {
    Y() noexcept;
    ~Y() {}
};

// 存在 guard
Y& y_normal_static() {
    static Y data;
    return data;
}

struct Z {
    constexpr Z() {}
    constexpr ~Z() {}
};

// GCC 有 guard，Clang 没有
Z& z_normal_static() {
    static Z data;
    return data;
}

// 只保留静态初始化，语义上不需要 guard
// 但是 GCC 有 guard，Clang 没有
Z& z_constinit_static() {
    static constinit Z data;
    return data;
}
