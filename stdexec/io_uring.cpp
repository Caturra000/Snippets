#include <cstring>
#include <mutex>
#include <iostream>
#include <thread>
#include <ranges>
#include <liburing.h>
#include <stdexec/execution.hpp>

// Operations in stdexec are required to be address-stable.
struct immovable {
    immovable() = default;
    immovable(immovable &&) = delete;
};

struct io_uring_exec: immovable {
    io_uring_exec(size_t uring_entries, int uring_flags = 0) {
        if(int err = io_uring_queue_init(uring_entries, &_underlying_uring, uring_flags)) {
            throw std::runtime_error(::strerror(-err));
        }
    }

    ~io_uring_exec() {
        io_uring_queue_exit(&_underlying_uring);
    }

    // (Receiver) Types erasure support.
    template <typename Result>
    struct vtab {
        using result_t = Result;
        virtual void complete(result_t) = 0;
    };

    // All the tasks are asynchronous.
    // The `task` struct is queued by a user-space intrusive queue.
    // NOTE: The io_uring-specified task is queued by an interal ring of io_uring.
    struct task: immovable, vtab<decltype(std::ignore)> {
        void complete(result_t) override {}
        task *next{this};
    };

    // Required by stdexec.
    template <stdexec::receiver Receiver>
    struct operation: task {
        using operation_state_concept = stdexec::operation_state_t;
        operation(Receiver receiver, io_uring_exec *uring)
            : receiver(std::move(receiver)), uring(uring) {}
        void start() noexcept {
            uring->push(this);
        }
        void complete(result_t) override {
            stdexec::set_value(std::move(receiver));
        }
        Receiver receiver;
        io_uring_exec *uring;
    };

    // Required by stdexec.
    struct sender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
                                        stdexec::set_value_t(),
                                        stdexec::set_error_t(std::exception_ptr)>;
        template <stdexec::receiver Receiver>
        operation<Receiver> connect(Receiver receiver) noexcept {
            return {std::move(receiver), uring};
        }
        io_uring_exec *uring;
    };

    // Required by stdexec.
    struct scheduler {
        io_uring_exec *uring;
        auto operator<=>(const scheduler &) const=default;
        sender schedule() noexcept { return {uring}; }
    };

    scheduler get_scheduler() noexcept { return {this}; }

    // External structured callbacks support.
    struct uring_operation {
        using result_t = decltype(std::declval<io_uring_cqe>().res);
        using base = vtab<result_t>;
    };

    void run() {
        for(task *first_task;;) {
            for(task *op = first_task = pop(); op; op = pop()) {
                op->complete({});
            }

            {
                std::lock_guard _{_mutex};
                // We didn't know how many inflight operations here.
                // See the comments on `coroutine.h`.
                // TL;DR: submitted seqs != inflight cqes
                /* int submitted_sqes = */ io_uring_submit(&_underlying_uring);
            }

            io_uring_cqe *cqe;
            unsigned head;
            unsigned done = 0;
            // Reap one operation / multiple operations.
            // NOTE: One sqe can generate multiple cqes.
            io_uring_for_each_cqe(&_underlying_uring, head, cqe) {
                done++;
                auto uring_op = std::bit_cast<uring_operation::base*>(cqe->user_data);
                uring_op->complete(cqe->res);
            }
            if(done) {
                io_uring_cq_advance(&_underlying_uring, done);
            } else if(!first_task) {
                std::this_thread::yield();
            }
        }
    }

    task* pop() noexcept {
        // Read only.
        if(_head.next == &_head) {
            return nullptr;
        }
        std::lock_guard _{_mutex};
        auto popped = std::exchange(_head.next, _head.next -> next);
        if(popped == _tail) _tail = &_head;
        return popped;
    }

    void push(task *op) noexcept {
        std::lock_guard _{_mutex};
        op->next = &_head;
        _tail = _tail->next = op;
    }

    task _head, *_tail{&_head};
    std::mutex _mutex;
    io_uring _underlying_uring;
};

template <auto F, stdexec::receiver Receiver, typename ...Args>
struct io_uring_exec_initiating_operation: immovable, io_uring_exec::uring_operation::base {
    using operation_state_concept = stdexec::operation_state_t;
    io_uring_exec_initiating_operation(Receiver receiver, io_uring_exec *uring,
                                       std::tuple<Args...> args)
        : receiver(std::move(receiver)),
          uring(uring),
          args(std::move(args)) {}

    void start() noexcept {
        if(auto sqe = io_uring_get_sqe(&uring->_underlying_uring)) [[likely]] {
            using operation_base = io_uring_exec::uring_operation::base;
            io_uring_sqe_set_data(sqe, static_cast<operation_base*>(this));
            std::apply(F, std::tuple_cat(std::tuple(sqe), std::move(args)));
        } else {
            stdexec::set_error(std::move(receiver), std::exception_ptr{});
        }
    }

    void complete(result_t cqe_res) override {
        // if(cqe->res == -ECANCELED) [[unlikely]] {
        //     stdexec::set_stopped(std::move(receiver));
        // } else {
            stdexec::set_value(std::move(receiver), cqe_res);
        // }
    }

    Receiver receiver;
    io_uring_exec *uring;
    std::tuple<Args...> args;
};

template <auto F, typename ...Args>
struct io_uring_exec_sender {
    using sender_concept = stdexec::sender_t;
    using completion_signatures = stdexec::completion_signatures<
                                    stdexec::set_value_t(io_uring_exec::uring_operation::result_t),
                                    stdexec::set_error_t(std::exception_ptr)>;

    template <stdexec::receiver Receiver>
    io_uring_exec_initiating_operation<F, Receiver, Args...>
    connect(Receiver receiver) noexcept {
        return {std::move(receiver), uring, std::move(args)};
    }

    io_uring_exec *uring;
    std::tuple<Args...> args;
};


// A sender factory.
template <auto F, typename ...Args>
stdexec::sender_of<stdexec::set_value_t(io_uring_exec::uring_operation::result_t /* cqe->res */)>
auto make_uring_sender(io_uring_exec::scheduler s, Args ...args) noexcept {
    return io_uring_exec_sender<F, Args...>{s.uring, std::tuple(std::move(args)...)};
}

// On  files  that  support seeking, if the `offset` is set to -1, the read operation commences at the file offset,
// and the file offset is incremented by the number of bytes read. See read(2) for more details. Note that for an
// async API, reading and updating the current file offset may result in unpredictable behavior, unless access to
// the file is serialized. It is **not encouraged** to use this feature, if it's possible to provide the  desired  IO
// offset from the application or library.
stdexec::sender
auto async_read(io_uring_exec::scheduler s, int fd, void *buf, size_t n, uint64_t offset = 0) noexcept {
    return make_uring_sender<io_uring_prep_read>(s, fd, buf, n, offset);
}

int main() {
    int fd = (unlink("/tmp/jojo"), open("/tmp/jojo", O_RDWR|O_TRUNC|O_CREAT));
    if(fd < 0 || write(fd, "dio", 3) != 3) {
        std::cerr << ::strerror(errno) , std::abort();
    }

    io_uring_exec uring(512);
    auto scheduler = uring.get_scheduler();
    auto s =
        stdexec::schedule(scheduler) | stdexec::then([] {
            std::cout << "hello ";
            return 19260816;
        })
      | stdexec::let_value([](int v) {
            return stdexec::just(v+1);
        }); // or then(...)

    auto s2 =
        stdexec::schedule(scheduler) | stdexec::then([] {
            std::cout << "world!" << std::endl;
            return std::array<char, 5>{};
        })
      | stdexec::let_value([=](auto &buf) {
            return async_read(scheduler, fd, buf.data(), 3)
              | stdexec::then([&](auto nread) {
                    auto bview = buf | std::views::take(nread);
                    for(auto b : bview) std::cout << b;
                    std::cout << std::endl;
                    return nread;
                });
        });
    std::jthread j {[&] { uring.run(); }};
    auto a = stdexec::when_all(s, s2);
    auto [v1, v2] = stdexec::sync_wait(std::move(a)).value();
    std::cout << "ans: " << v1 << ' ' << v2 << std::endl;
}
