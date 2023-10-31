// 示例来自：https://godbolt.org/z/dnrabsbdq
// 一个低配版的std::execution实现
// 我在知乎翻译了相关的文章：https://zhuanlan.zhihu.com/p/664212982

/*
 *  Copyright 2022 NVIDIA Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

// This is a toy implementation of the core parts of the C++ std::execution
// proposal (aka, Executors, http://wg21.link/P2300). It is intended to be a
// learning tool only. THIS CODE IS NOT SUITABLE FOR ANY USE.

#include <condition_variable>
#include <cstdio>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <iostream>

// Some utility code
///////////////////////////////////////////
struct immovable {
    immovable() = default;
    immovable(immovable&&) = delete;
};

template <class S, class R>
using connect_result_t = decltype(connect(std::declval<S>(), std::declval<R>()));

template <class S>
using sender_result_t = typename S::result_t;

///////////////////////////////////////////
// just(T) sender factory
///////////////////////////////////////////

template <class R, class T>
struct just_operation : immovable {
    R rec;
    T value;

    friend void start(just_operation& self) {
        set_value(self.rec, self.value);
    }
};

template <class T>
struct just_sender {
    using result_t = T;
    T value;

    template <class R>
    friend just_operation<R, T> connect(just_sender self, R rec) {
        return {{}, rec, self.value};
    }
};

template <class T>
just_sender<T> just(T t) {
    return {t};
}

///////////////////////////////////////////
// then(Sender, Function) sender adaptor
///////////////////////////////////////////

template <class R, class F>
struct then_receiver {
    R rec;
    F f;

    friend void set_value(then_receiver self, auto val) {
        set_value(self.rec, self.f(val));
    }

    friend void set_error(then_receiver self, std::exception_ptr err) {
        set_error(self.rec, err);
    }

    friend void set_stopped(then_receiver self) {
        set_stopped(self.rec);
    }
};

template <class S, class R, class F>
struct then_operation : immovable {
    connect_result_t<S, then_receiver<R, F>> op;

    friend void start(then_operation& self) {
        start(self.op);
    }
};

template <class S, class F>
struct then_sender {
    using result_t = std::invoke_result_t<F, sender_result_t<S>>;
    S s;
    F f;

    template <class R>
    friend then_operation<S, R, F> connect(then_sender self, R rec) {
        return {{}, connect(self.s, then_receiver<R, F>{rec, self.f})};
    }
};

template <class S, class F>
then_sender<S, F> then(S s, F f) {
    return {s, f};
}

//////////////////////////////////////////
// sync_wait() sender consumer
///////////////////////////////////////////
struct sync_wait_data {
    std::mutex mtx;
    std::condition_variable cv;
    std::exception_ptr err;
    bool done = false;
};

template <class T>
struct sync_wait_receiver {
    sync_wait_data& data;
    std::optional<T>& value;

    friend void set_value(sync_wait_receiver self, auto val) {
        std::unique_lock lk{self.data.mtx};
        self.value.emplace(val);
        self.data.done = true;
        self.data.cv.notify_one();
    }

    friend void set_error(sync_wait_receiver self, std::exception_ptr err) {
        std::unique_lock lk{self.data.mtx};
        self.data.err = err;
        self.data.done = true;
        self.data.cv.notify_one();
    }

    friend void set_stopped(sync_wait_receiver self) {
        std::unique_lock lk{self.data.mtx};
        self.data.done = true;
        self.data.cv.notify_one();
    }
};

template <class S>
std::optional<sender_result_t<S>> sync_wait(S s) {
    using T = sender_result_t<S>;
    sync_wait_data data;
    std::optional<T> value;

    auto op = connect(s, sync_wait_receiver<T>{data, value});
    start(op);

    std::unique_lock lk{data.mtx};
    data.cv.wait(lk, [&]{return data.done;});

    if (data.err)
        std::rethrow_exception(data.err);

    return value;
}


///////////////////////////////////////////
// run_loop execution context
///////////////////////////////////////////
struct run_loop : immovable {
    struct none{};

    struct task : immovable {
        task* next = this;
        virtual void execute() {}
    };

    template <class R>
    struct operation : task {
        R rec;
        run_loop& loop;

        operation(R rec, run_loop& loop)
          : rec(rec), loop(loop) {}

        void execute() override final {
            set_value(rec, none{});
        }

        friend void start(operation& self) {
            self.loop.push_back(&self);
        }
    };

    task head;
    task* tail = &head;
    bool finishing = false;
    std::mutex mtx;
    std::condition_variable cv;

    void push_back(task* op) {
        std::unique_lock lk(mtx);
        op->next = &head;
        tail = tail->next = op;
        cv.notify_one();
    }

    task* pop_front() {
        std::unique_lock lk(mtx);
        cv.wait(lk, [this]{return head.next != &head || finishing;});
        if (head.next == &head)
            return nullptr;
        return std::exchange(head.next, head.next->next);
    }

    struct sender {
        using result_t = none;
        run_loop* loop;

        template <class R>
        friend operation<R> connect(sender self, R rec) {
            return {rec, *self.loop};
        }
    };

    struct scheduler {
        run_loop* loop;
        friend sender schedule(scheduler self) {
            return {self.loop};
        }
    };

    scheduler get_scheduler() {
        return {this};
    }

    void run() {
        while (auto* op = pop_front())
            op->execute();
    }

    void finish() {
        std::unique_lock lk(mtx);
        finishing = true;
        cv.notify_all();
    }
};

///////////////////////////////////////////
// thread_context execution context
///////////////////////////////////////////
class thread_context : run_loop {
    std::thread th{[this]{ run(); }};
public:
    using run_loop::get_scheduler;
    using run_loop::finish;
    void join() {
        th.join();
    }
};


//
// start test code
//
struct cout_receiver {
    friend void set_value(cout_receiver self, auto val) {
        std::cout << "Result: " << val << '\n';
    }

    friend void set_error(cout_receiver self, std::exception_ptr err) {
        std::terminate();
    }

    friend void set_stopped(cout_receiver self) {
        std::terminate();
    }
};

int main() {

    // auto s = just(42);
    // auto op = connect(s, cout_receiver{});
    // start(op);


    // auto s2 = then(just(42), [](int i) { return i + 1; });
    // auto op2 = connect(s2, cout_receiver{});
    // start(op2);


    // auto s3 = then(just(42), [](int i) { return i + 1; });
    // int val3 = sync_wait(s3).value();
    // std::cout << "Result: " << val3 << '\n';


    // run_loop loop;
    // auto sched4 = loop.get_scheduler();
    // auto s4 = then(schedule(sched4), [](auto) { return 42; });
    // auto op4 = connect(s4, cout_receiver{});
    // start(op4);
    // auto s5 = then(schedule(sched4), [](auto) { return 43; });
    // auto op5 = connect(s5, cout_receiver{});
    // start(op5);

    // loop.finish();
    // loop.run();


    thread_context th;
    auto sched6 = th.get_scheduler();

    auto s6 = then(schedule(sched6), [](auto) { return 42; });
    auto s7 = then(s6, [](int i){return i+1;});
    auto val7 = sync_wait(s7).value();
    th.finish();
    th.join();

    std::cout << val7 << '\n';
}
