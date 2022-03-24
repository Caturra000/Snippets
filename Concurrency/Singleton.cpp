#include <bits/stdc++.h>
using namespace std;

class Singleton {
public:
    static Singleton* instance() {
        // 可以进一步放宽使用consume
        auto ptr = _instance.load(std::memory_order_acquire);
        if(!ptr) {
            std::lock_guard<std::mutex> _ {_mutex};
            // 错误示范
            // ptr = new Singleton();
            // _instance.store(ptr, std::memory_order_release);
            //
            // 记住，再load一遍
            // Question. 在上锁的前提这里能不能直接用relaxed？
            // 似乎不行，因为instance()一开始的load就是在锁外部的
            // 但是这种属于“有关联”的program order，CPU也会乱序执行吗？
            ptr = _instance.load(std::memory_order_acquire);
            if(!ptr) {
                ptr = new Singleton();
                _instance.store(ptr, std::memory_order_release);
            }
        }
        // 因为要求dereference必须要happens-after于其它线程的初始化
        // 所以创建实例后要release，并且dereference前要acquire/consume
        return ptr;
    }

private:
    static std::atomic<Singleton*> _instance;
    static std::mutex _mutex;
};

std::atomic<Singleton*> Singleton::_instance {nullptr};
std::mutex Singleton::_mutex;

int main() {
    auto instance = Singleton::instance();
    return 0;
}