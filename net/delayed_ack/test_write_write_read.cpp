#include "bits/stdc++.h"
#include "../fluent/fluent.hpp"

// 测试write-write-read模型
// 参考40ms magic number
// https://www.cnblogs.com/wajika/p/6573028.html

// 测试结果是lo设备下并不会有delayed ack
// TODO 看来需要找个内网IP测一下

void server() {
    fluent::InetAddress address {INADDR_ANY, 2569};
    fluent::Server server {address};
    server.onMessage([](fluent::Context *context) {
        auto stream = context->input.unread();
        // hardcode: head + body
        if(stream >= 4) {
            context->input.hasRead(4);
            context->send("response");
        } else {
            std::cerr << "input: " << stream << std::endl;
        }
    });
    server.ready();
    server.run();
}

void client() {
    const char head[] = "h";
    const char body[] = "b";

    fluent::InetAddress address {"127.0.0.1", 2569};
    fluent::Socket socket;
    socket.setBlock();
    // 错误处理就免了，看抓包就行
    socket.connect(address);
    // 构造write-write-read模型
    char respose[0xff];
    socket.write(head, sizeof head);
    socket.write(body, sizeof body);
    ::read(socket.fd(), respose, sizeof respose);
    std::cout << "done" << std::endl;
    for(int c; std::cin >> c;);
}

int main() {
    std::thread background {server};
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    client();
    background.join();
    return 0;
}