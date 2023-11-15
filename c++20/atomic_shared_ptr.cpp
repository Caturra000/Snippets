#include <memory>
#include <atomic>
#include <cassert>

// 测试atomic<shared_ptr>是否lockfree
// 注：g++-11不能编译，g++-12才可以
//
// 有大佬指出：https://lrita.github.io/2020/11/10/cpp-atomic-sharedptr/
// libstdc++居然是用mutex来实现原子性，非常难绷
//
// 使用godbolt测试过，即使是目前最新的clang17/gcc13，都返回false
int main() {
    using namespace std;
    using ass = atomic<shared_ptr<size_t>>;

    // "false"
    constexpr bool test_constexpr = ass::is_always_lock_free;
    // static_assert(test_constexpr);

    ass val {nullptr};
    bool test_runtime = val.is_lock_free();

    // "false"
    assert(test_runtime);
}
