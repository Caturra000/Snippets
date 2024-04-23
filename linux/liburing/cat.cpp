#include <unistd.h>
#include <fcntl.h>
#include <liburing.h>
#include <format>
#include <iostream>
#include <ranges>
#include <string_view>
#include <vector>
#include <memory>
#include <algorithm>
#include <cassert>
#include "utils.h"

// Return {fd, size_bytes} for regular files.
auto get_file_info(std::string_view file_path) {
    auto fd = open(file_path.data(), O_RDONLY) | nofail("open");

    struct stat stat;
    fstat(fd, &stat) | nofail("fstat");

    return std::tuple(fd, stat.st_size);
}

auto make_iovecs_with_buf(size_t size_bytes, size_t chunk_size) {
    std::vector<iovec> iovecs;
    std::vector<std::byte> buf(size_bytes);
    assert(chunk_size > 0);
    size_t chunks = (size_bytes + chunk_size - 1) / chunk_size;
    size_t offset = 0;
    std::generate_n(std::back_inserter(iovecs), chunks, [&] {
        iovec iov;
        iov.iov_base = &buf[offset];
        iov.iov_len = std::min(size_bytes - offset, chunk_size);
        offset += iov.iov_len;
        return iov;
    });
    return std::tuple(std::move(iovecs), std::move(buf));
}

void print_to_stdout(const auto &iovecs, size_t size_bytes, size_t chunk_size) {
    assert(chunk_size > 0);
    auto chunks = (size_bytes + chunk_size - 1) / chunk_size;
    assert(chunks == iovecs.size());
    auto printer = std::ostream_iterator<char>{std::cout};
    for(auto &iov : iovecs) {
        std::copy_n(std::bit_cast<char*>(iov.iov_base), iov.iov_len, printer);
    }
}

int main(int argc, char *argv[]) {
    if(argc < 2) {
        std::cerr << std::format("Usage: {} [file name] <[file name] ...>\n", argv[0]);
        return 1;
    }

    constexpr size_t ENTRIES = 1;
    constexpr size_t CHUNK_SIZE = 64;

    io_uring uring;
    io_uring_queue_init(ENTRIES, &uring, 0);
    auto uring_cleanup = defer([&](...) {
        io_uring_queue_exit(&uring); 
    });

    auto filenames = std::ranges::subrange(argv+1, argv+argc);
    for(auto filename : filenames) {
        auto [fd, size] = get_file_info(filename);
        auto fd_cleanup = defer([&](...) { close(fd); });
        auto [iovecs, buf] = make_iovecs_with_buf(size, CHUNK_SIZE);

        auto sqe = io_uring_get_sqe(&uring);
        io_uring_prep_readv(sqe, fd, iovecs.data(), iovecs.size(), 0);

        // User data...
        // io_uring_sqe_set_data(sqe, nullptr);

        // Synchronized.
        // io_uring_submit_and_wait(&uring, 1);

        io_uring_submit(&uring) | nofail<std::less_equal<int>>("io_uring_submit");

        // Still synchronized...
        io_uring_cqe *cqe;
        io_uring_wait_cqe(&uring, &cqe) | nofail("io_uring_wait_cqe");
        cqe->res | nofail("readv");
        auto cqe_cleanup = defer([&](...) { io_uring_cqe_seen(&uring, cqe); });

        print_to_stdout(iovecs, cqe->res, CHUNK_SIZE);
    }

    return 0;
}
