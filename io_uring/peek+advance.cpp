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

    /////////////////////////////////////////////////////////////////////////// submit

    auto _flush_submit = [&] {
        unsigned tail = sq.sqe_tail;
        bool sqpoll = p.flags & IORING_SETUP_SQPOLL;
        if(sq.sqe_head != tail) {
            sq.sqe_head = tail;
            if(sqpoll) {
                smp_store_release(sq.p_tail, tail);
            } else {
                *sq.p_tail = tail;
            }
        }
        unsigned head = std::invoke([&] {
            if(sqpoll) return READ_ONCE(*sq.p_head);
            return *sq.p_head;
            
        });
        return tail - head;
    };

    auto submit_and_wait = [&](unsigned wait_nr) -> int {
        unsigned submit_nr = _flush_submit();

        bool sq_need_enter = std::invoke([&] {
            if(!submit_nr) return false;
            if(!(p.flags & IORING_SETUP_SQPOLL)) return true;
            // TODO: SQPOLL wakeup flag! Also NEED a store operation!
            return false;
        });
        bool cq_need_enter = std::invoke([&] {
            if(wait_nr) return true;
            if(p.flags & IORING_SETUP_IOPOLL) return true;
            auto need_flush = IORING_SQ_CQ_OVERFLOW | IORING_SQ_TASKRUN; 
            if(READ_ONCE(*sq.p_flags) & need_flush) return true;
            return false;
        });
        auto enter_flags = std::invoke([&] {
            unsigned int enter_flags = 0;
            if(false /* TODO: use_IORING_REGISTER_RING_FDS */) {
                enter_flags |= IORING_ENTER_REGISTERED_RING;
            }
            if(cq_need_enter) {
                enter_flags |= IORING_ENTER_GETEVENTS;
            }
            return enter_flags;
        });

        if(sq_need_enter || cq_need_enter) {
            return io_uring_enter(fd, submit_nr, wait_nr, enter_flags);
        }
        return submit_nr;
    };

    auto submit = [&] { return submit_and_wait(0); };

    int cnt = submit() | nofail("submit");

    assert(cnt == 3);

    /////////////////////////////////////////////////////////////////////////// peek

    auto cq_ready = [&] {
        return smp_load_acquire(cq.p_tail) - *cq.p_head;
    };

    auto peek_batch_cqe = [&](io_uring_cqe **cqe_ptrs, unsigned count) {
        assert(count > 0);
        auto get_ready = [&] { return std::min(cq_ready(), count); };
        unsigned ready = get_ready();
        if(ready == 0) {
            auto need_flush = IORING_SQ_CQ_OVERFLOW | IORING_SQ_TASKRUN;
            if(READ_ONCE(*sq.p_flags) & need_flush) {
                io_uring_enter(fd, 0, 0, IORING_ENTER_GETEVENTS);
            }
            // Can be 0 again.
            ready = get_ready();
        }
        unsigned head = *cq.p_head;
        unsigned last = head + ready;
        unsigned mask = cq.ring_mask;
        for(auto i = 0u; head != last; i++, head++) {
            cqe_ptrs[i] = &cq.cqes[head & mask];
        }
        return ready;
    };

    auto cq_advance = [&](unsigned nr) {
        if(nr == 0) return;
        smp_store_release(cq.p_head, *cq.p_head + nr);
    };

    std::array<io_uring_cqe*, 3> cqes;
    for(cnt = 0; cnt != 3;) {
        cnt += peek_batch_cqe(cqes.data() + cnt, cnt - cqes.size());
    }
    cq_advance(3);

    system("cat /tmp/jojo");
}
