#include <bits/stdc++.h>

struct Spinlock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;

    // 问题：x86下即使全用relaxed似乎也没问题？
    bool tryLock() { return !flag.test_and_set(std::memory_order_acquire); }
    void lock() { while(flag.test_and_set(std::memory_order_acquire)); }
    void unlock() { flag.clear(std::memory_order_release); }
};

int gValue {0};
Spinlock gLock;


int main() {
    auto incr = [&](int count) {
        while(count-- > 0) {
            std::lock_guard<Spinlock> _ {gLock};
            gValue++;
        }
    };
    std::thread t1 {incr, 10000000};
    std::thread t2 {incr, 20000000};
    t1.join();
    t2.join();
    std::cout << gValue << std::endl;
}