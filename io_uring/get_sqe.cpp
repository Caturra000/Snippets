#include "io_uring.hpp"

constexpr unsigned queue_depth = 32;

// Setup stage is simplified from `setup.cpp`.
int main() {
    /////////////////////////////////////////////////////////////////////////// setup

    io_uring_params p {};
    int fd = io_uring_setup(queue_depth, &p) | nofail("setup");

    void *sqring_addr {};
    void *cqring_addr {};
    void *sqes_addr {};
    // liburing uses sizeof(unsigned).
    auto sqring_size = p.sq_off.array + p.sq_entries * sizeof(unsigned);
    sqring_addr = mmap(nullptr, sqring_size,
                            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
                            fd, IORING_OFF_SQ_RING);
    assert(sqring_addr != MAP_FAILED);

    auto sqe_size = sizeof(io_uring_sqe);
    auto sqes_size = sqe_size * p.sq_entries;
    sqes_addr = mmap(nullptr, sqes_size,
                            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
                            fd, IORING_OFF_SQES);
    assert(sqes_addr != MAP_FAILED);

    assert(p.features & IORING_FEAT_SINGLE_MMAP);
    cqring_addr = sqring_addr;
    assert(cqring_addr != MAP_FAILED);

    assert(sqring_addr && cqring_addr && sqes_addr);

    SQ_ref sq;
    CQ_ref cq;

    sq.p_head       = mmap_start_lifetime_as<unsigned>(sqring_addr, p.sq_off.head);
    sq.p_tail       = mmap_start_lifetime_as<unsigned>(sqring_addr, p.sq_off.tail);
    sq.p_flags      = mmap_start_lifetime_as<unsigned>(sqring_addr, p.sq_off.flags);
    sq.p_dropped    = mmap_start_lifetime_as<unsigned>(sqring_addr, p.sq_off.dropped);
    sq.ring_mask    = *mmap_start_lifetime_as<unsigned>(sqring_addr, p.sq_off.ring_mask);
    sq.ring_entries = *mmap_start_lifetime_as<unsigned>(sqring_addr, p.sq_off.ring_entries);
    sq.sqes         = mmap_start_lifetime_as_array<io_uring_sqe>(sqes_addr, 0, sq.ring_entries);
    sq.p_array      = mmap_start_lifetime_as_array<unsigned>(
                        sqring_addr, p.sq_off.array, sq.ring_entries);

    namespace stdv = std::views;
    namespace stdr = std::ranges;

    auto array_view = stdv::iota(sq.p_array)
                    | stdv::take(sq.ring_entries)
                    | stdv::transform([](auto *i) -> decltype(auto) { return *i; });
    // Make sequence.
    for(auto i{0u}; auto &idx : array_view) idx = i++; 

    cq.p_head       = mmap_start_lifetime_as<unsigned>(cqring_addr, p.cq_off.head);
    cq.p_tail       = mmap_start_lifetime_as<unsigned>(cqring_addr, p.cq_off.tail);
    cq.p_flags      = mmap_start_lifetime_as<unsigned>(cqring_addr, p.cq_off.flags);
    cq.p_overflow   = mmap_start_lifetime_as<unsigned>(cqring_addr, p.cq_off.overflow);
    cq.ring_mask    = *mmap_start_lifetime_as<unsigned>(cqring_addr, p.cq_off.ring_mask);
    cq.ring_entries = *mmap_start_lifetime_as<unsigned>(cqring_addr, p.cq_off.ring_entries);
    cq.cqes         = mmap_start_lifetime_as_array<io_uring_cqe>(cqring_addr, p.cq_off.cqes, cq.ring_entries);

    /////////////////////////////////////////////////////////////////////////// prepare a test file

    constexpr char jojo_path[] = "/tmp/jojo";
    unlink(jojo_path);
    int jojo_fd = open(jojo_path, O_RDWR|O_TRUNC|O_CREAT|O_APPEND, 0666) | nofail("open");

    /////////////////////////////////////////////////////////////////////////// get

    auto get_sqe = [&] {
        unsigned int head, next = sq.sqe_tail + 1;
        if(p.flags & IORING_SETUP_SQPOLL) head = smp_load_acquire(sq.p_head);
        else                              head = *sq.p_head;
        if(next - head > sq.ring_entries) return (io_uring_sqe*){};
        auto sqe = &sq.sqes[sq.sqe_tail & sq.ring_mask];
        sq.sqe_tail = next;
        memset(sqe, 0, sizeof(io_uring_sqe));
        return sqe;
    };

    auto prepare_write = [jojo_fd]<size_t N>(io_uring_sqe *sqe, const char (&buf)[N]) {
        assert(sqe);
        sqe->opcode = IORING_OP_WRITE;
        sqe->fd = jojo_fd;
        sqe->addr = std::bit_cast<decltype(sqe->addr)>(&buf);
        sqe->len = N - 1;
    };

    assert(*sq.p_tail == 0);
    
    prepare_write(get_sqe(), "hello ");
    prepare_write(get_sqe(), "world ");
    prepare_write(get_sqe(), "! ");

    // Tell the kernel we have updated the tail pointer.
    smp_store_release(sq.p_tail, 3);

    int cnt = io_uring_enter(fd, 3, 3, IORING_ENTER_GETEVENTS) | nofail("submit");
    assert(smp_load_acquire(sq.p_head) == *sq.p_tail);
    assert(*sq.p_head == 3);
    assert(cnt == 3);

    system("cat /tmp/jojo");
}
