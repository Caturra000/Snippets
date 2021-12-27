#include <bits/stdc++.h>

struct Spinlock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;

    bool tryLock() { return !flag.test_and_set(); }
    void lock() { while(flag.test_and_set()); }
    void unlock() { flag.clear(); }
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