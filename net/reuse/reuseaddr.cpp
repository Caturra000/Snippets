#include <bits/stdc++.h>
#include "../fluent/fluent.hpp"

int main() {
    fluent::InetAddress address {INADDR_ANY, 2563};
    try {
        // 默认已经配置好REUSEADDR（其实也配上了REUSEPORT，这不是关键，你可以在库里面注释掉）
        // server构造时只经历bind，不会进入LISTEN，因此重复bind是没问题的
        // 题外话：如果是只配置REUSEPORT，也不会有异常
        fluent::Server server1 {address};
        fluent::Server server2 {address};
    } catch(const fluent::FluentException &e) {
        // 如果在fluent库中关闭REUSEADDR，那么将会抛出异常
        // 换句话说就是系统调用会返回errno
        // Address already in use
        std::cerr << e.what() << std::endl;
    }
    return 0;
}