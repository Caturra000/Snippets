#pragma once
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <linux/version.h>
#include <linux/io_uring.h>
#include <bits/stdc++.h>

#define HAS_USER_ADDR (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0))

//////////////////////////////////////////////////////////// Syscall wrappers

int io_uring_setup(unsigned entries, struct io_uring_params *p) {
    return (int) syscall(__NR_io_uring_setup, entries, p);
}

int io_uring_enter(int ring_fd, unsigned int to_submit,
    unsigned int min_complete, unsigned int flags) {
    return (int) syscall(__NR_io_uring_enter, ring_fd, to_submit, min_complete,
                   flags, NULL, _NSIG / 8);
}

//////////////////////////////////////////////////////////// Library interface to io_uring

struct SQ_ref {
    unsigned *p_head;
    unsigned *p_tail;
    unsigned *p_flags;
    unsigned *p_dropped;
    unsigned *p_array;
    struct io_uring_sqe *sqes;

    unsigned sqe_head;
    unsigned sqe_tail;

    size_t ring_sz;
    void *ring_ptr;

    unsigned ring_mask;
    unsigned ring_entries;
};

struct CQ_ref {
    unsigned *p_head;
    unsigned *p_tail;
    unsigned *p_flags;
    unsigned *p_overflow;
    struct io_uring_cqe *cqes;

    size_t ring_sz;
    void *ring_ptr;

    unsigned ring_mask;
    unsigned ring_entries;
};


//////////////////////////////////////////////////////////// Helpers

// C++-style check for syscall.
// Failed on ret < 0 by default.
//
// INT_ASYNC_CHECK: helper for liburing (-ERRNO) and other syscalls (-1).
// It may break generic programming (forced to int).
template <typename Comp = std::less<int>, auto V = 0, bool INT_ASYNC_CHECK = true>
struct nofail {
    std::string_view reason;

    // Examples:
    // fstat(...) | nofail("fstat");        // Forget the if-statement and ret!
    // int fd = open(...) | nofail("open"); // If actually need a ret, here you are!
    friend decltype(auto) operator|(auto &&ret, nofail nf) {
        if(Comp{}(ret, V)) [[unlikely]] {
            // Hack errno.
            if constexpr (INT_ASYNC_CHECK) {
                using T = std::decay_t<decltype(ret)>;
                static_assert(std::is_convertible_v<T, int>);
                // -ERRNO
                if(ret != -1) errno = -ret;
            }
            perror(nf.reason.data());
            std::terminate();
        }
        return std::forward<decltype(ret)>(ret);
    };
};

// Make clang happy.
nofail(...) -> nofail<std::less<int>, 0, true>;

auto _dump(io_sqring_offsets &off) {
    std::cout << "("
              << "head: " << off.head << ", "
              << "tail: " << off.tail << ", "
              << "ring_mask: " << off.ring_mask << ", "
              << "ring_entries: " << off.ring_entries << ", "
              << "flags: " << off.flags << ", "
              << "dropped: " << off.dropped << ", "
              << "array: " << off.array << ", "
#if HAS_USER_ADDR
              << "user_addr: " << off.user_addr
#endif
              << ")"
              << std::endl;
    return "";
}

auto _dump(io_cqring_offsets &off) {
    std::cout << "("
              << "head: " << off.head << ", "
              << "tail: " << off.tail << ", "
              << "ring_mask: " << off.ring_mask << ", "
              << "ring_entries: " << off.ring_entries << ", "
              << "overflow: " << off.overflow << ", "
              << "cqes: " << off.cqes << ", "
              << "flags: " << off.flags << ", "
#if HAS_USER_ADDR
              << "user_addr: " << off.user_addr
#endif
              << ")"
              << std::endl;
    return "";
}

void dump(io_uring_params &params) {
    std::cout << "("
              << "sq_entries: " << params.sq_entries << ", "
              << "cq_entries: " << params.cq_entries << ", "
              << "flags: " << params.flags << ", "
              << "features: " << params.features << ", "
              << "wq_fd: " << params.wq_fd
              << "\n\tsq_off: " << _dump(params.sq_off)
              << "\tcq_off: " << _dump(params.cq_off)
              << ")"
              << std::endl;
}

// Simply copy from:
// https://stackoverflow.com/questions/79164176/emulate-stdstart-lifetime-as-array-in-c20
template<class T>
requires (std::is_trivially_copyable_v<T> /* && std::is_implicit_lifetime_v<T> */)
T* start_lifetime_as(void* p) noexcept {
    return std::launder(static_cast<T*>(std::memmove(p, p, sizeof(T))));
}

template<class T>
requires (std::is_trivially_copyable_v<T> /* && std::is_implicit_lifetime_v<T> */)
T* start_lifetime_as_array(void* p, std::size_t n) noexcept {
    if(n == 0)
        return static_cast<T*>(p);
    else
        return std::launder(static_cast<T*>(std::memmove(p, p, sizeof(T)*n)));
}

template <typename T>
auto mmap_start_lifetime_as(void *mmap_start, unsigned offset) {
    auto ptr_val = std::bit_cast<std::uintptr_t>(mmap_start) + offset;
    return start_lifetime_as<T>(std::bit_cast<T*>(ptr_val));
}

template <typename T>
auto mmap_start_lifetime_as_array(void *mmap_start, unsigned offset, std::size_t n) {
    auto ptr_val = std::bit_cast<std::uintptr_t>(mmap_start) + offset;
    return start_lifetime_as_array<T>(std::bit_cast<T*>(ptr_val), n);
}

// Copy from liburing.
template <typename T>
void WRITE_ONCE(T &var, auto val) {
    std::atomic_store_explicit(reinterpret_cast<std::atomic<T> *>(&var),
                    val, std::memory_order_relaxed);
}

template <typename T>
T READ_ONCE(const T &var) {
    return std::atomic_load_explicit(
        reinterpret_cast<const std::atomic<T> *>(&var),
        std::memory_order_relaxed);
}

template <typename T>
void smp_store_release(T *p, auto v) {
    std::atomic_store_explicit(reinterpret_cast<std::atomic<T> *>(p), v,
                    std::memory_order_release);
}

template <typename T>
T smp_load_acquire(const T *p) {
    return std::atomic_load_explicit(
        reinterpret_cast<const std::atomic<T> *>(p),
        std::memory_order_acquire);
}
