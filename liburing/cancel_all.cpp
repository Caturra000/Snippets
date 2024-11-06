#include <chrono>
#include <iostream>
#include <liburing.h>
#include <errno.h>
#include <string.h>
#include "utils.h"

auto to_kernel_ts(std::chrono::milliseconds duration) {
    using namespace std::chrono;
    auto duration_s = duration_cast<seconds>(duration);
    auto duration_ns = duration_cast<nanoseconds>(duration - duration_s);
    auto ts = __kernel_timespec {
        .tv_sec = duration_s.count(),
        .tv_nsec = duration_ns.count()
    };
    return ts;
}

int main() {
    constexpr size_t ENTRIES = 32;

    io_uring uring;
    io_uring_queue_init(ENTRIES, &uring, 0);
    auto uring_cleanup = defer([&](...) {
        io_uring_queue_exit(&uring); 
    });

    io_uring_sqe* timer_sqes[5];

    for(auto &sqe : timer_sqes) {
        sqe = io_uring_get_sqe(&uring);
        (!!sqe) | nofail<std::equal_to<>>("timer sqe");
        // 避免user data == NULL
        io_uring_sqe_set_data(sqe, sqe);
        std::cout << "SQE allocated!" << std::endl;
    }

    for(auto sqe : timer_sqes) {
        using namespace std::chrono_literals;
        auto ts = to_kernel_ts(999s);
        io_uring_prep_timeout(sqe, &ts, 0, 0);
        std::cout << "timeout() is ready!" << std::endl;
    }

    // int n = 0;
    int n = io_uring_submit(&uring);
    std::cout << "submit: " << n << std::endl;

    // io_uring_sqe *cancel_sqes[10];
    io_uring_sqe *cancel_sqes[1];

    for(auto &sqe : cancel_sqes) {
        sqe = io_uring_get_sqe(&uring);
        (!!sqe) | nofail<std::equal_to<>>("cancel sqe");
        std::cout << "SQE allocated!" << std::endl;
    }

    for(auto sqe : cancel_sqes) {
        // man文档也说了，ALL指的是匹配user data的情况
        // 但是我以为ANY是取消任意一个操作，没想到是所有
        // 后续这个操作会收割到cqe->res == 5，也就是前面timer全被取消了
        io_uring_prep_cancel(sqe, nullptr, IORING_ASYNC_CANCEL_ANY);
        std::cout << "cancel() is ready!" << std::endl;
    }

    // 如果timer和cancel合并提交也不会改变结果
    // sqe ringbuffer内部自行维护顺序
    n = io_uring_submit(&uring);
    std::cout << "submit: " << n << std::endl;

    for(io_uring_cqe *cqe; !io_uring_peek_cqe(&uring, &cqe);) {
        std::cout << "res: " << cqe->res;
        if(cqe->res < 0) {
            std::cout << " " << strerror(-cqe->res);
        }
        std::cout << std::endl;
        
        io_uring_cqe_seen(&uring, cqe);
    }
}
