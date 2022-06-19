#include "bits/stdc++.h"
#include "../../fluent/fluent.hpp"

// 一个非常简单的SYN-RST例子
// 其实可以更简单点，直接连server都不bind了

// 客户端使用nc
// $ nc localhost 2568
int main() {
    fluent::InetAddress address {INADDR_ANY, 2568};
    fluent::Server server {address};
    // 不进行server.ready()，既不启用listen初始化连接队列
    // 这样client握手会直接RST拒绝
    // server.ready();
    server.run();
    return 0;
}
