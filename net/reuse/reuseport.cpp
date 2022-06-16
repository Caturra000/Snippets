#include <bits/stdc++.h>
#include "../fluent/fluent.hpp"

int main() {
    fluent::InetAddress address {INADDR_ANY, 2564};
    try {
        // 需要修改fluent库，设置为只配置REUSEPORT
        // 可以看到，同时绑定并LISTEN都不会引起冲突
        //
        // 如果只配置REUSEADDR，那么只要server1执行了bind+listen
        // 后面server2并不能完成bind
        fluent::Server server1 {address};
        server1.ready();
        fluent::Server server2 {address};
        server2.ready();
    } catch(const fluent::FluentException &e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}