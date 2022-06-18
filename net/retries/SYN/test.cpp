#include <bits/stdc++.h>
// #ifdef SOMAXCONN
// #undef SOMAXCONN
// #define SOMAXCONN 1
// #endif
#include "../../fluent/fluent.hpp"

// SOMAXCONN不好处理，直接修改内核参数方便点
// $ sysctl -w net.core.somaxconn=1
// 这样处理后不管listen配置多大backlog
// 总会在少数连接建立后，后续的连接都塞不下半连接队列

// 简单测试是前2个连接能立刻完成握手
// 后面由于队列满了，server又不accept捞出连接，导致不再对往后的连接进行ACK答复
//
// 为什么设了somaxconn=1会有2个完成握手呢？（既能到达全连接队列）
// TODO 好问题，有空琢磨下


// 不直接使用fluent::Client
// 因为fluent::Client是完全异步的
// 而且client.connect()还默认处理了各种异常和重试，会干扰整个过程
// 这种测试场合需要同步阻塞
// 所以使用更底层的fluent::Socket
void client() {
    fluent::Socket s;
    fluent::InetAddress address {"127.0.0.1", 2567};
    std::vector<fluent::Socket> sockets(17);
    for(auto &socket : sockets) {
        socket.setBlock();
        if(socket.connect(address)) {
            std::cerr << "connect failed: " << strerror(errno) << std::endl;
        } else {
            // 持久累计client()调用次数
            static size_t count;
            std::cout << "done: " << ++count << std::endl;
        }
    }
}

int main(int argc, const char *argv[]) {

    fluent::InetAddress address {INADDR_ANY, 2567};
    // backlog默认就是SOMAXCONN
    // 太大了默认并不方便测试
    fluent::Server server {address};

    // 只开启listen构造连接队列，阻止server accept行为
    server.ready();
    // server.run();

    client();

    for(int c; std::cin >> c;);
    return 0;
}
