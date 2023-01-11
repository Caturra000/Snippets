#include <bits/stdc++.h>

// 本文件示范如何在并发过程中正确地执行swap操作

// 假设这是正常的实现，无需并发支持
class Some_big_object { /*...*/ };
void swap(Some_big_object &, Some_big_object &) { /*...*/ }

// X类会在并发环境下使用
class X {
public:
    X(const Some_big_object &object): _some_detail(object) {}

    friend void swap(X &lhs, X &rhs) {
        if(&lhs == &rhs) return;
        // 基本原则：按固定的顺序获取，以避免死锁
        //
        // Note:
        // std::lock可能抛出异常，但保证不泄露锁资源
        // 要么全部成功，要么全部失败
        std::lock(lhs._mutex, rhs._mutex);
        // 使用lock_guard支持RAII
        // 使用adopt_lock表明锁资源在此前已经获取
        std::lock_guard<std::mutex> _[] {
            {lhs._mutex, std::adopt_lock},
            {rhs._mutex, std::adopt_lock},
        };

        // Note: 也可以直接用std::scoped_lock来完成上面的操作

        using std::swap;
        swap(lhs._some_detail, rhs._some_detail);
    }

private:
    Some_big_object _some_detail;
    std::mutex _mutex;
};

int main() {
    // TODO 一个并发case
    Some_big_object b;
    X x(b);
    X y(b);
    swap(x, y);
}