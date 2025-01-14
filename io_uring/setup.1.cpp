#include <sys/mman.h>
#include <fcntl.h>
#include "io_uring.hpp"
#include "app.hpp"

int main() {
    io_uring_params p {};
    int fd = io_uring_setup(128, &p) | nofail("setup");
    dump(p);

    assert(p.features & IORING_FEAT_SINGLE_MMAP);

    // liburing uses sizeof(unsigned).
    auto ring_size = p.sq_off.array + p.sq_entries * sizeof(unsigned);
    void *ring_addr = mmap(nullptr, ring_size,
                            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
                            fd, IORING_OFF_SQ_RING);
    assert(ring_addr != MAP_FAILED);

    auto sqe_size = sizeof(io_uring_sqe);
    auto sqes_size = sqe_size * p.sq_entries;
    void *sqes_addr = mmap(nullptr, sqes_size,
                            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
                            fd, IORING_OFF_SQES);
    assert(sqes_addr != MAP_FAILED);

    SQ_ref sq;
    CQ_ref cq;

    sq.p_head       = mmap_start_lifetime_as<unsigned>(ring_addr, p.sq_off.head);
    sq.p_tail       = mmap_start_lifetime_as<unsigned>(ring_addr, p.sq_off.tail);
    sq.p_flags      = mmap_start_lifetime_as<unsigned>(ring_addr, p.sq_off.flags);
    sq.p_dropped    = mmap_start_lifetime_as<unsigned>(ring_addr, p.sq_off.dropped);
    sq.ring_mask    = *mmap_start_lifetime_as<unsigned>(ring_addr, p.sq_off.ring_mask);
    sq.ring_entries = *mmap_start_lifetime_as<unsigned>(ring_addr, p.sq_off.ring_entries);
    sq.p_array      = mmap_start_lifetime_as_array<unsigned>(ring_addr, p.sq_off.array, sq.ring_entries);
    sq.sqes         = mmap_start_lifetime_as_array<io_uring_sqe>(sqes_addr, 0, sq.ring_entries);

    std::cout << sq.ring_mask << std::endl;
    std::cout << sq.ring_entries << std::endl;
    std::cout << *sq.p_head << std::endl;
    std::cout << *sq.p_tail << std::endl;
    auto sqes_indics = std::views::iota(0) | std::views::take(sq.ring_entries);
    for(auto i : sqes_indics) {
        std::cout << sq.p_array[i] << ' ';
    }
    std::cout << std::endl;

    cq.p_head       = mmap_start_lifetime_as<unsigned>(ring_addr, p.cq_off.head);
    cq.p_tail       = mmap_start_lifetime_as<unsigned>(ring_addr, p.cq_off.tail);
    cq.p_flags      = mmap_start_lifetime_as<unsigned>(ring_addr, p.cq_off.flags);
    cq.p_overflow   = mmap_start_lifetime_as<unsigned>(ring_addr, p.cq_off.overflow);
    cq.ring_mask    = *mmap_start_lifetime_as<unsigned>(ring_addr, p.cq_off.ring_mask);
    cq.ring_entries = *mmap_start_lifetime_as<unsigned>(ring_addr, p.cq_off.ring_entries);
    cq.cqes         = mmap_start_lifetime_as_array<io_uring_cqe>(ring_addr, p.cq_off.cqes, cq.ring_entries);

    std::cout << cq.ring_mask << std::endl;
    std::cout << cq.ring_entries << std::endl;

    int jojo_fd = (::unlink("/tmp/jojo"), open("/tmp/jojo", O_RDWR|O_TRUNC|O_CREAT|O_APPEND, 0666))
                | nofail("open");

    auto prepare_write = [jojo_fd](io_uring_sqe *sqe, const char *str) {
        sqe->opcode = IORING_OP_WRITE;
        sqe->fd = jojo_fd;
        sqe->addr = std::bit_cast<decltype(sqe->addr)>(str);
        sqe->len = strlen(str);
    };

    assert(*sq.p_tail == 0);
    prepare_write(&sq.sqes[0], "hello ");
    prepare_write(&sq.sqes[1], "world ");
    prepare_write(&sq.sqes[2], "! ");
    // write_barrier(); // TODO: need impl.

    *sq.p_tail += 3;
    assert(*sq.p_tail == 3);

    // Test out-of-order submission feature.
    for(auto i : sqes_indics | std::views::take(3)) {
        sq.p_array[i] = 2 - i;
    }

    io_uring_enter(fd, 3, 3, IORING_ENTER_GETEVENTS) | nofail("submit");
    assert(*sq.p_head == *sq.p_tail);

    system("cat /tmp/jojo");
}
