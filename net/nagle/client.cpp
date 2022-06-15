#include <bits/stdc++.h>

#include "../fluent/fluent.hpp"

int main(int argc, const char *argv[]) {
    if(argc <= 2) {
        std::cerr << "usage: " << argv[0]
                  << "host_ip tcp_no_delay(bool)" << std::endl;
        return -1;
    }

    dlog::Log::init();
    fluent::FLUENT_LOG_INFO("[client]", "log init");

    const char *serverIp = argv[1];
    const uint16_t knownPort = 2560;

    bool noDelay = ::atoi(argv[2]);

    fluent::InetAddress address {serverIp, knownPort};
    fluent::Client client;
    size_t onFlight = 1;
    auto future = client.connect(address)
        .then([&](fluent::Context *context) {
            onFlight--;
            if(noDelay) context->socket.setNoDelay();
            context->send("x");
            fluent::FLUENT_LOG_INFO("[client]", "write 1 byte");
            return nullptr;
        });

    auto nop = fluent::makeFuture(client.looper(), nullptr)
        .poll([&](nullptr_t) {
            using namespace std::chrono_literals;
            if(onFlight) std::this_thread::sleep_for(1s);
            return false;
        });
    client.run();
    return 0;
}
