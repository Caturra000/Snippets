#include "io_uring.hpp"

// Kernel v6.6+
constexpr bool enable_IORING_SETUP_NO_SQARRAY = false;
// Kernel v6.5+
constexpr bool enable_IORING_SETUP_NO_MMAP = false;

#ifndef IORING_SETUP_NO_SQARRAY
#define IORING_SETUP_NO_SQARRAY (1U << 16)
#endif

#ifndef IORING_SETUP_NO_MMAP
#define IORING_SETUP_NO_MMAP (1U << 14)
#endif

int main() {
    io_uring_params p {};
    if constexpr(enable_IORING_SETUP_NO_SQARRAY) {
        p.flags |= IORING_SETUP_NO_SQARRAY;
    }
    if constexpr (enable_IORING_SETUP_NO_MMAP) {
        p.flags |= IORING_SETUP_NO_MMAP;
        // TODO: need allocation. We cannot allocate buffer from userspace directly.
        // `man 2 io_uring_setup`:
        //   Typically, callers should allocate this memory by using mmap(2)  to  allocate  a
        //   huge  page.

        /// WRONG!
#if HAS_USER_ADDR
        // void *sq_cq_buf;
        // void *sqes_buf;
        // posix_memalign(&sq_cq_buf, 16384, 16384);
        // posix_memalign(&sqes_buf, 16384, 16384);
        // assert(sq_cq_buf);
        // assert(sqes_buf);
        // p.sq_off.user_addr = std::bit_cast<unsigned long long>(&sq_cq_buf);
        // p.cq_off.user_addr = std::bit_cast<unsigned long long>(&sqes_buf);
#endif
    }

    int fd = io_uring_setup(128, &p) | nofail("setup");
    dump(p);

    // liburing uses sizeof(unsigned).
    auto sqring_size = p.sq_off.array + p.sq_entries * sizeof(unsigned);
    void *sqring_addr = mmap(nullptr, sqring_size,
                            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
                            fd, IORING_OFF_SQ_RING);
    assert(sqring_addr != MAP_FAILED);

    auto sqe_size = sizeof(io_uring_sqe);
    auto sqes_size = sqe_size * p.sq_entries;
    void *sqes_addr = mmap(nullptr, sqes_size,
                            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
                            fd, IORING_OFF_SQES);
    assert(sqes_addr != MAP_FAILED);

    auto mmap_cqring_opt = [=] {
        if(p.features & IORING_FEAT_SINGLE_MMAP) {
            return sqring_addr;
        }
        auto cqring_size = p.cq_off.cqes + p.cq_entries * sizeof(struct io_uring_cqe);
        return mmap(nullptr, cqring_size,
                    PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
                    fd, IORING_OFF_CQ_RING);
    };
    void *cqring_addr = mmap_cqring_opt();
    assert(cqring_addr != MAP_FAILED);

    SQ_ref sq;
    CQ_ref cq;

    sq.p_head       = mmap_start_lifetime_as<unsigned>(sqring_addr, p.sq_off.head);
    sq.p_tail       = mmap_start_lifetime_as<unsigned>(sqring_addr, p.sq_off.tail);
    sq.p_flags      = mmap_start_lifetime_as<unsigned>(sqring_addr, p.sq_off.flags);
    sq.p_dropped    = mmap_start_lifetime_as<unsigned>(sqring_addr, p.sq_off.dropped);
    sq.ring_mask    = *mmap_start_lifetime_as<unsigned>(sqring_addr, p.sq_off.ring_mask);
    sq.ring_entries = *mmap_start_lifetime_as<unsigned>(sqring_addr, p.sq_off.ring_entries);
    sq.sqes         = mmap_start_lifetime_as_array<io_uring_sqe>(sqes_addr, 0, sq.ring_entries);
    if(!(p.flags & IORING_SETUP_NO_SQARRAY)) {
        sq.p_array  = mmap_start_lifetime_as_array<unsigned>(
                        sqring_addr, p.sq_off.array, sq.ring_entries);
    } // Otherwise sqarray has overlapped memory layout.

    std::cout << sq.ring_mask << std::endl;
    std::cout << sq.ring_entries << std::endl;
    std::cout << *sq.p_head << std::endl;
    std::cout << *sq.p_tail << std::endl;

    namespace stdv = std::views;
    namespace stdr = std::ranges;

    auto array_view = stdv::iota(sq.p_array)
                    | stdv::take(sq.ring_entries)
                    | stdv::transform([](auto *i) -> decltype(auto) { return *i; });
    if(!(p.flags & IORING_SETUP_NO_SQARRAY)) {
        auto is_0 = [](auto i) { return !i; };
        assert(stdr::all_of(array_view, is_0));
    }

    cq.p_head       = mmap_start_lifetime_as<unsigned>(cqring_addr, p.cq_off.head);
    cq.p_tail       = mmap_start_lifetime_as<unsigned>(cqring_addr, p.cq_off.tail);
    cq.p_flags      = mmap_start_lifetime_as<unsigned>(cqring_addr, p.cq_off.flags);
    cq.p_overflow   = mmap_start_lifetime_as<unsigned>(cqring_addr, p.cq_off.overflow);
    cq.ring_mask    = *mmap_start_lifetime_as<unsigned>(cqring_addr, p.cq_off.ring_mask);
    cq.ring_entries = *mmap_start_lifetime_as<unsigned>(cqring_addr, p.cq_off.ring_entries);
    cq.cqes         = mmap_start_lifetime_as_array<io_uring_cqe>(cqring_addr, p.cq_off.cqes, cq.ring_entries);

    std::cout << cq.ring_mask << std::endl;
    std::cout << cq.ring_entries << std::endl;

    constexpr char jojo_path[] = "/tmp/jojo";
    unlink(jojo_path);
    int jojo_fd = open(jojo_path, O_RDWR|O_TRUNC|O_CREAT|O_APPEND, 0666) | nofail("open");

    auto prepare_write = [jojo_fd]<size_t N>(io_uring_sqe *sqe, const auto (&buf)[N]) {
        memset(sqe, 0, sizeof(io_uring_sqe));
        sqe->opcode = IORING_OP_WRITE;
        sqe->fd = jojo_fd;
        sqe->addr = std::bit_cast<decltype(sqe->addr)>(&buf);
        sqe->len = N - 1;
    };

    assert(*sq.p_tail == 0);
    prepare_write(&sq.sqes[0], "hello ");
    prepare_write(&sq.sqes[1], "world ");
    prepare_write(&sq.sqes[2], "! ");

    if(!(p.flags & IORING_SETUP_NO_SQARRAY)) {
        // Test out-of-order submission feature.
        auto reversed_array_view = array_view | stdv::take(3) | stdv::reverse;
        for(auto i{0u}; auto &idx : reversed_array_view) {
            idx = i++;
        }
    } // Otherwise sqarray has no mapping.

    smp_store_release(sq.p_tail, 3);

    io_uring_enter(fd, 3, 3, IORING_ENTER_GETEVENTS) | nofail("submit");
    assert(smp_load_acquire(sq.p_head) == *sq.p_tail);
    assert(*sq.p_head == 3);

    system("cat /tmp/jojo");
}
