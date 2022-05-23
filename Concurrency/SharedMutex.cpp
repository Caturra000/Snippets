#include <bits/stdc++.h>

// 参考6.S081单变量实现
// 其实应该叫SharedSpin
class SharedMutex {
public:
    void lock();
    void unlock();
    void lockShared();
    void unlockShared();

private:
    // n == 0：未上锁
    // n == -1：被一个writer持有
    // n > 0：被n个reader持有
    std::atomic<int> _n {0};
};

// 我在这一版的实现还是用了do-while（觉得更适合CAS），可读性可能上一版好一点（实现见上一条commit，更详细注释见上上条commit）
// 千万记得do-while别用continue，用得少还是掉了坑
inline void SharedMutex::lockShared() {
    int old;
    do {
        while((old = _n.load(std::memory_order_relaxed)) < 0);
    } while(!_n.compare_exchange_weak(old, old+1, std::memory_order_acquire, std::memory_order_relaxed));
}

inline void SharedMutex::unlockShared() {
    int old;
    do {
        while((old = _n.load(std::memory_order_relaxed)) <= 0);
    } while(!_n.compare_exchange_weak(old, old-1, std::memory_order_release, std::memory_order_relaxed));
}

inline void SharedMutex::lock() {
    int old;
    do {
        old = 0;
    } while(!_n.compare_exchange_strong(old, -1, std::memory_order_acquire, std::memory_order_relaxed));
}

inline void SharedMutex::unlock() {
    int old;
    do {
        old = -1;
    } while(!_n.compare_exchange_strong(old, 0, std::memory_order_release, std::memory_order_relaxed));
}

int main() {
    int val = 0;
    SharedMutex mutex;
    constexpr static size_t COUNT = 1e4;
    auto writer = [&] {
        for(int i = 0; i < COUNT; ++i) {
            mutex.lock();
            val++;
            mutex.unlock();
        }
    };
    // test deadlock
    auto nop = [&] {
        for(int i = 0; i < COUNT; ++i) {
            mutex.lockShared();
            mutex.unlockShared();
        }
    };
    std::vector<std::thread> workers;
    for(int count = 8; count--;) workers.emplace_back(writer);
    for(int count = 8; count--;) workers.emplace_back(nop);
    for(auto &&worker: workers) worker.join();
    std::cout << val << std::endl;
    return 0;
}
