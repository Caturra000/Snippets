#include <bits/stdc++.h>

#include "../fluent/fluent.hpp"

// 这里包含2个测试：
// 1. nagle和nodelay的正常行为
// 2. nagle本身发送是否有时间上的延迟阈值，还是必须等待ack到来

int main(int argc, const char *argv[]) {
    if(argc <= 2) {
        std::cerr << "usage: " << argv[0]
                  << "host_ip tcp_no_delay(bool)" << std::endl;
        return -1;
    }

    dlog::Log::init();
    fluent::FLUENT_LOG_INFO("[client]", "log init");

    int hang;

    const char *serverIp = argv[1];
    const uint16_t knownPort = 2560;

    bool noDelay = ::atoi(argv[2]);
    if(noDelay) {
        std::cout << "enable no_delay" << std::endl;
    } else {
        std::cout << "enable nagle" << std::endl;
    }

    fluent::InetAddress address {serverIp, knownPort};

    fluent::Socket socket;
    socket.setBlock();
    if(noDelay) socket.setNoDelay();

    fluent::FLUENT_LOG_INFO("[client]", "set blocking connect");
    if(socket.connect(address)) {
        std::cerr << "connect failed: " << strerror(errno) << std::endl;
        ::abort();
    }
    fluent::FLUENT_LOG_INFO("[client]", "connected");

    // 这里手动hang，可以后期搭配iptables测试，将ESTABLISHED server的ack都拦截下来
    // 这样就能测出nagle是否会有一个最长的等待合并阈值
    // TODO
    std::cin >> hang;

    for(size_t count {10}; count--;) {
        char c = 'x';
        int n = ::write(socket.fd(), &c, 1);
        fluent::FLUENT_LOG_INFO("[client]", "write byte:", n);
        if(n < 0) {
            // 日志只用于跟踪用户层连接和传输的时间点
            // 错误就不写到日志里了
            std::cerr << "write failed: " << strerror(errno) << std::endl;
        }
    }

    std::cin >> hang;
    return 0;
}
