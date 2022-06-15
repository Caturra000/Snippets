#include <bits/stdc++.h>
#include "../fluent/fluent.hpp"

// 客户端直接使用nc
// $ nc localhost 2562
// 输出过程中直接ctrl + C
// - 不满意的地方是挥手过程仍然是完整的

void stupidWrite(int fd) {
    char c = 'x';
    if(::write(fd, &c, 1) < 0) {
        std::cerr << "write failed: " << strerror(errno) << std::endl;
    }
}

void fillAndSleep(fluent::Context *context) {
    constexpr size_t loop = 1e3;
    using Tuple = std::tuple<fluent::Context*, fluent::StrongLifecycle>;
    context->ensureLifecycle();
    context->makeStrongFuture()
        .poll(loop, [](Tuple tup) {
            auto ctx = std::get<0>(tup);

            // case1: 不受SIGPIPE影响，也不引起signal回调
            // - 因为挥手过程是完整的，我写的状态转移都考虑好了
            // - 但是我还没测过不完整的情况
            ctx->send("x");
            // case2: 不用signal处理的话直接挂于写时信号，否则每一次write都引起woo回调并抛出Broken pipe
            // stupidWrite(ctx->socket.fd());

            // 如果手速不够快的话就跑慢点
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            return false;
        })
        .then([](Tuple tup) {
            std::cout << "finish" << std::endl;
            return nullptr;
        });
}

void server() {
    fluent::InetAddress address {INADDR_ANY, 2562};
    fluent::Server server {address};

    server.onConnect([](fluent::Context *context) {
        std::cout << "connected" << std::endl;
        fillAndSleep(context);
    });
    server.onClose([](fluent::Context*) {
        std::cout << "closed" << std::endl;
    });
    server.ready();
    server.run();
}

int main(int argc, const char *argv[]) {

    ::signal(SIGPIPE, +[](int) {
        std::cerr << "woo" << std::endl;
    });

    server();
    for(int c; std::cin >> c;);
    return 0;
}
