#pragma once
#include <liburing.h>
#include <memory>
#include <algorithm>
#include <coroutine>
#include <queue>
#include <utility>
#include <thread>
#include <chrono>
#include <algorithm>
#include <ranges>
#include "utils.h"

struct Task {
    struct promise_type;
    Task() = default;
    constexpr Task(std::coroutine_handle<promise_type> handle) noexcept: _handle(handle) {}
    // Task is not intended to be a lvalue and handle/promise is already allocated whenenver used.
    // But in the default dtor, we cannot handle this case, so it results in a memory leak.
    // NOTE: co_await operator is not allowed to aceept lvalue Task, so why are you doing this?
    ~Task() = default;
    // No move/copy.
    Task(const Task&) = delete;
    Task(Task&&) = delete;
    Task& operator=(const Task&) = delete;
    Task& operator=(Task&&) = delete;

    std::coroutine_handle<promise_type> _handle;
};

struct Task::promise_type {
    constexpr auto initial_suspend() const noexcept { return std::suspend_always{}; }
    constexpr void return_void() const noexcept {}
    void unhandled_exception() {/*TODO*/}
    Task get_return_object() noexcept {
        auto h = std::coroutine_handle<promise_type>::from_promise(*this);
        return {h};
    }
    struct Final_suspend {
        constexpr bool await_ready() const noexcept { return false; }
        auto await_suspend(auto h) const noexcept {
            auto next = h.promise()._parent;
            // Started task (at least once) will kill itself in final_suspend.
            h.destroy();
            return next;
        }
        // Never reached.
        constexpr auto await_resume() const noexcept {}
    };
    constexpr auto final_suspend() const noexcept { return Final_suspend{}; }
    void push(std::coroutine_handle<> parent) noexcept { _parent = parent; }

    std::coroutine_handle<> _parent {std::noop_coroutine()};
};

// Multi-task support.
inline auto operator co_await(Task &&task) noexcept {
    struct awaiter {
        bool await_ready() const noexcept { return !_handle || _handle.done(); }
        auto await_suspend(std::coroutine_handle<> current) noexcept {
            _handle.promise().push(current);
            // Multi-tasks are considered as a single operation in io_contexts.
            return _handle;
        }
        constexpr auto await_resume() const noexcept {}

        std::coroutine_handle<Task::promise_type> _handle;
    };
    return awaiter{task._handle};
}

struct Async_user_data {
    io_uring *uring;
    io_uring_sqe *sqe;
    io_uring_cqe *cqe;
    std::coroutine_handle<> h;
};

// Currently `Result` is unused.
template <typename Result>
struct Async_operation {
    constexpr bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) {
        user_data.h = h;
        io_uring_sqe_set_data(user_data.sqe, &user_data);
        io_uring_submit(user_data.uring) | nofail<std::less_equal<int>>("io_uring_submit");
    }
    // TODO: Don't return cqe->res directly.
    auto await_resume() const noexcept {
        return user_data.cqe->res;
    }
    Async_operation(io_uring *uring, auto uring_prep_fn, auto &&...args) {
        user_data.uring = uring;
        user_data.sqe = io_uring_get_sqe(uring);
        uring_prep_fn(user_data.sqe, std::forward<decltype(args)>(args)...);
    }

    Async_user_data user_data;
};

inline auto async_operation(io_uring *uring, auto uring_prep_fn, auto &&...args) {
    using Result = std::invoke_result_t<decltype(uring_prep_fn), io_uring_sqe*, decltype(args)...>;
    return Async_operation<Result>(uring, uring_prep_fn, std::forward<decltype(args)>(args)...);
}

// A quite simple io_context.
class Io_context {
public:
    Io_context(io_uring &uring): uring(uring) {}
    void run() {
        constexpr auto PLUG_MAX = 32;
        auto runtime_plug = [&] { return std::min<int>(PLUG_MAX, _operations.size()); };
        while(!_stop || (_stop && (!_operations.empty()))) {
            for(auto plug : std::views::iota(0, runtime_plug())) {
                std::ignore = plug;
                auto h = _operations.front();
                _operations.pop();
                h.resume();
            }

            // Some operations are in-flight,
            // even if we currently have no any h.resume().
            // Just continue along the path!

            io_uring_cqe *cqe;
            unsigned head;
            unsigned done = 0;
            // Reap one operation / multiple operations.
            // NOTE: One operation can also generate multiple cqes (awaiters).
            io_uring_for_each_cqe(&uring, head, cqe) {
                done++;
                auto user_data = std::bit_cast<Async_user_data*>(cqe->user_data);
                user_data->cqe = cqe;
                user_data->h.resume();
            }
            done ? io_uring_cq_advance(&uring, done) : hang();
        }
    }

    // Some in-flight operations will be suspended when calling stop().
    // It is the responsibility of users to ensure the correctness of this function.
    // Or you can re-run() agagin to get thing done.
    void done() { _stop = true; }

    friend void co_spawn(Io_context &io_context, Task &&task) {
        io_context._operations.emplace(std::exchange(task._handle, {}));
    }

private:
    void hang() {
        // TODO: config option, event driven.
        constexpr bool ENABLE_BUSY_LOOP = false;
        if constexpr (!ENABLE_BUSY_LOOP) {
            // FIXME: yield() in a single thread makes no sense.
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1ns);
        }
    }

    io_uring &uring;
    std::queue<std::coroutine_handle<>> _operations;
    bool _stop {false};
};

inline auto async_accept(io_uring *uring, int server_fd,
        sockaddr *addr, socklen_t *addrlen, int flags = 0) {
    return async_operation(uring,
        io_uring_prep_accept, server_fd, addr, addrlen, flags);
}

inline auto async_accept(io_uring *uring, int server_fd, int flags = 0) {
    return async_operation(uring,
        io_uring_prep_accept, server_fd, nullptr, nullptr, flags);
}

inline auto async_read(io_uring *uring, int fd, void *buf, size_t n, int flags = 0) {
    return async_operation(uring,
        io_uring_prep_read, fd, buf, n, flags);
}


inline auto async_write(io_uring *uring, int fd, const void *buf, size_t n, int flags = 0) {
    return async_operation(uring,
        io_uring_prep_write, fd, buf, n, flags);
}

inline auto async_close(io_uring *uring, int fd) {
    return async_operation(uring,
        io_uring_prep_close, fd);
}
