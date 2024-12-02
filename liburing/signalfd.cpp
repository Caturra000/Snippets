#include <sys/signalfd.h>
#include <poll.h>
#include <liburing.h>
#include <signal.h>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <memory>
#include <array>
#include <iostream>
#include <ranges>
#include <thread>

// Just copy and paste.
[[nodiscard("defer() is not allowed to be temporary.")]]
inline auto defer(auto func) {
    auto _0x1 = std::uintptr_t {0x1};
    // reinterpret_cast is not a constexpr.
    auto make_STL_happy = reinterpret_cast<void*>(_0x1);
    auto make_dtor_happy = [f = std::move(func)](...) { f(); };
    using Defer = std::unique_ptr<void, decltype(make_dtor_happy)>;
    return Defer{make_STL_happy, std::move(make_dtor_happy)};
}

// Block all signals in ctor.
// Restore previous signals in dtor (or .reset()).
inline auto signal_blocker() {
    sigset_t new_mask, old_mask;
    sigfillset(&new_mask);
    sigemptyset(&old_mask);
    // The use of sigprocmask() is unspecified in a multithreaded process; see pthread_sigmask(3).
    pthread_sigmask(SIG_BLOCK, &new_mask, &old_mask);

    return defer([old_mask] {
        pthread_sigmask(SIG_SETMASK, &old_mask, 0);
    });
}

inline auto make_signalfd_from = [](auto signals) {
    sigset_t mask;
    sigemptyset(&mask);
    for(auto signal : signals) sigaddset(&mask, signal);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);
    auto fd = signalfd(-1, &mask, 0);
    if(fd < 0) throw std::system_error(errno, std::system_category());
    return fd; // No RAII.
};

// 似乎这种做法只能用在单线程，多线程有点莫名其妙了
// Thread semantics
//     The  semantics of signalfd file descriptors in a multithreaded program mirror the standard seman‐
//     tics for signals.  In other words, when a thread reads from a signalfd file descriptor,  it  will
//     read  the signals that are directed to the thread itself and the signals that are directed to the
//     process (i.e., the entire thread group).  (A thread will not be able to read signals that are di‐
//     rected to other threads in the process.)
int main() {
    auto sb = signal_blocker();
    constexpr size_t ENTRIES = 32;
    io_uring uring;
    io_uring_queue_init(ENTRIES, &uring, 0);
    auto uring_cleanup = defer([&] {
        io_uring_queue_exit(&uring); 
    });

    // 不使用额外线程可以做到信号处理
    std::thread t {[&] {
        auto sfd = make_signalfd_from(std::array {SIGINT});
        auto sqe = io_uring_get_sqe(&uring);
        io_uring_prep_poll_add(sqe, sfd, POLLIN);
        int n = io_uring_submit(&uring);
        std::cout << "submit: " << n << std::endl;
    }};

    t.join();

    std::array<io_uring_cqe*, 64> cqes;

    int n = 0;

    // Press Ctrl + C to break this loop.
    for(;;) {
        n = io_uring_peek_batch_cqe(&uring, cqes.data(), cqes.size());
        if(n) {
            std::cout << "peek: " << n << std::endl;
            // 懒得回收cqe了
            break;
        }
    }

    for(auto cqe : cqes | std::views::take(n)) {
        std::cout << cqe->res << std::endl;
        // 可能是Operation canceled
        if(cqe->res < 0) std::cerr << strerror(-cqe->res) << std::endl;
    }
}