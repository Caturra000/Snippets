#include <bits/stdc++.h>
#include "fluent.hpp"

// 0: shutdown
// 1: close
int main(int argc, const char *argv[]) {

    bool forced = false;
    if(argc <= 2 && argv[1][0] == '1') {
        forced = true;
        std::cout << "enable forced mode" << std::endl;
    } else {
        std::cout << "disable forced mode" << std::endl;
    }

    fluent::InetAddress address {"127.0.0.1", 2565};
    fluent::Client client;

    client.connect(address, [forced](fluent::Context *context) {
        if(forced) {
            ::close(context->socket.fd());
            context->socket.detach();
        } else {
            context->shutdown();
        }
        return "404notfound";
    });

    client.run();
    return 0;
}