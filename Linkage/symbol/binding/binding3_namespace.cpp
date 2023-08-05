// 注意，在这里使用readelf可能符号会被截断（原因是名字太长了）
// 因此需要加上--wide参数

inline namespace {
    // LOCAL
    void func() {}
}

inline namespace named {
    // GLOBAL
    void func_named() {}
}

namespace {
    // LOCAL
    void func_anon() {}
}
