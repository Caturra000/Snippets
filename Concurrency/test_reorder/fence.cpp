#include <iostream>
#include <thread>
#include <atomic>

int x = 0;
int y = 0;

int r0 = 100;
int r1 = 100;

void f1() {
    x = 1;
    asm volatile("mfence": : :"memory");
    r0 = y;
}

void f2() {
    y = 1;
    asm volatile("mfence": : :"memory");
    r1 = x;
}

void init() {
    x = 0;
    y = 0;
    r0 = 100;
    r1 = 100;
}

bool check() {
    return r0 == 0 && r1 == 0;
}

std::atomic<bool> wait1{true};
std::atomic<bool> wait2{true};
std::atomic<bool> stop{false};

void loop1() {
    while(!stop.load(std::memory_order_relaxed)) {
        while (wait1.load(std::memory_order_relaxed));

        // asm volatile("" ::: "memory");
        f1();
        // asm volatile("" ::: "memory");

        wait1.store(true, std::memory_order_relaxed);
    }
}

void loop2() {
    while (!stop.load(std::memory_order_relaxed)) {
        while (wait2.load(std::memory_order_relaxed));

        // asm volatile("" ::: "memory");
        f2();
        // asm volatile("" ::: "memory");

        wait2.store(true, std::memory_order_relaxed);
    }
}

int main() {
    std::thread thread1(loop1);
    std::thread thread2(loop2);

    long count = 0;
    while(true) {
        count++;
        init();
        asm volatile("" ::: "memory");
        wait1.store(false, std::memory_order_relaxed);
        wait2.store(false, std::memory_order_relaxed);

        while (!wait1.load(std::memory_order_relaxed));
        while (!wait2.load(std::memory_order_relaxed));
        asm volatile("" ::: "memory");
        if (check()) {
            std::cout << "test count " << count << ": r0 == " << r0 << " && r1 == " << r1 << std::endl;
            break;
        } else {
            // if (count % 10000 == 0) {
            //     std::cout << "test count " << count << ": OK" << std::endl;
            // }
            if(count >= 2013/*0000*/) {
                std::cout << "PASS" << std::endl;
                break;
            }
        }
    }

    stop.store(true);
    wait1.store(false);
    wait2.store(false);
    thread1.join();
    thread2.join();
    return 0;
}