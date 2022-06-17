#include <bits/stdc++.h>
#define private public
#define protected public
#include "fluent.hpp"
#undef protected
#undef private

int main() {
    fluent::InetAddress address {INADDR_ANY, 2565};
    fluent::Server server {address};

    server.onConnect([&](fluent::Context* context) {
        // 强制移除网络状态机
        // 这样client发出shutdown指令后，server也不会有任何行为，因此server不会发出FIN
        // （由于我写fluent库时根本不想开放这些破坏状态的接口，因此直接硬改private/protected权限了）
        //
        // 抓包看出某种协议栈实现是FW2阶段的FIN无响应后直接等120s（恰好是RTO MAX），然后client发出RST
        // 看样子FW2确实没有retry策略
        context->_nState = fluent::NetworksPolicy::NetworkState::DISCONNECTED;
    });

    server.ready();
    server.run();
    return 0;
}