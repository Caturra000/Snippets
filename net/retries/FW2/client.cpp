#include <bits/stdc++.h>
#include "fluent.hpp"

int main() {
    fluent::InetAddress address {INADDR_ANY, 2565};
    fluent::Client client;

    client.connect(address, [](fluent::Context *context) {
        // send FIN
        context->shutdown();
        return "404notfound";
    });

    client.run();
    return 0;
}