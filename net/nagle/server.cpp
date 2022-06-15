#include "bits/stdc++.h"
#include "../fluent/fluent.hpp"

int main() {

    dlog::Log::init();
    fluent::FLUENT_LOG_INFO("[server]", "log init");

    fluent::InetAddress address {INADDR_ANY, 2560};
    fluent::Server server {address};

    server.onConnect([](fluent::Context *context) {
        fluent::FLUENT_LOG_INFO("[server]", "recv", context->simpleInfo());
    });

    server.onMessage([](fluent::Context *context) {
        fluent::FLUENT_LOG_INFO("[server]", "mesg", context->simpleInfo());
    });

    // 关了ready()可以用于模拟SYN-RST
    server.ready();
    server.run();
    return 0;
}