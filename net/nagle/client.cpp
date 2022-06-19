#include <bits/stdc++.h>

#include "../fluent/fluent.hpp"

// 这里包含2个测试：
// 1. nagle和nodelay的正常行为
// 2. nagle本身发送是否有时间上的延迟阈值，还是必须等待ack到来

// 对于测试1：
// nagle算法首次write仍然会立刻PUSH，后续则尽可能等待ACK后才发出（对端server的ACK足够快的前提下）
// - seq 1:2
// - ack 2
// - seq 2:11
// - ack 11
// nodelay就没啥好说了，每次都是尽快PUSH，但是偶尔也有合并
// - seq 1:2
// - seq 2:6
// - seq 6:11
// - ack 6
// - ack 11

// 对于测试2：
// TL;DR 存在时间上的阈值，大约278ms
//
// 过程：
// 比较麻烦，需要配合iptables
// $ iptables -A INPUT -p tcp --sport 2560 --tcp-flags ALL ACK -j DROP
// （在连接建立后再使用）
// 另外一个麻烦事是tcpdump抓包是发生在iptables前的，因此即使iptables过滤了ACK包，tcpdump仍然会抓到
// 但是协议栈处理确实是受到影响的
//
// 感受下时间点吧：
// - 14:24:29.255335 seq 1:2
// - 14:24:29.533625 seq 2:11
// - 14:24:29.903419 seq 1:11
// - 14:24:30.693472 seq 1:11
// - 14:24:32.213518 seq 1:11
// - ...略
//
// 第一次发送也是立刻PUSH，后续无法等待ACK到来后把剩下的已合并字节（不包含首次发送）都一起发送了
// 这个阈值大概在278ms量级
// 自第三次发送开始，seq完整合并为1:11，但是后续的重传都是按照RTO翻倍处理，已经是正常的超时重传阶段了
// 进入正常重传阶段的阈值大概在370ms量级

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
