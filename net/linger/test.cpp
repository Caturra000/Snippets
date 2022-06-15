#include <bits/stdc++.h>
#include "../fluent/fluent.hpp"

void client() {
    fluent::InetAddress address {"127.0.0.1", 2561};
    fluent::Client client;
    auto future = client.connect(address, [](fluent::Context *context) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            auto &socket = context->socket;
            int fd = socket.fd();
            linger l { .l_onoff = 1, .l_linger = 0 };
            if(::setsockopt(fd, SOL_SOCKET, SO_LINGER, &l, sizeof(l))) {
                std::cerr << "linger error\nreason: " << strerror(errno) << std::endl;
            }
            // Note1: 后面解释为啥要加这个
            for(size_t DUMMY = 1 << 10; DUMMY--;) context->send("x");
            // 优雅关闭
            context->shutdown();
            // 或者直接暴力关闭
            // socket.detach();
            // ::close(fd);

            // 1. wireshark抓包仍然可能会走非常礼貌的FIN四次挥手过程
            // - 可能的原因是，这里用的是lo设备，且缓冲区清空很快

            // 2. 但是添加Note1处的代码后会观测：
            // - 客户端发出FIN,ACK，对端ACK
            // - 然后客户端不管TW2，直接暴力RST,ACK

            // 3. 不管是shutdown还是close，都可以发出RST
            return nullptr;
        });
    client.run();
}

void server() {
    fluent::InetAddress address {INADDR_ANY, 2561};
    fluent::Server server {address};
    server.onConnect([](fluent::Context*) {
        std::cout << "connected" << std::endl;
    });
    server.onClose([](fluent::Context*) {
        std::cout << "closed" << std::endl;
    });
    server.ready();
    server.run();
}

int main(int argc, const char *argv[]) {
    std::thread {server}.detach();
    std::thread {client}.detach();
    for(int c; std::cin >> c;);
    return 0;
}
