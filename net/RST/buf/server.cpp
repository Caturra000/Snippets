#include <bits/stdc++.h>
#include "fluent.hpp"

int main() {
    fluent::InetAddress address {INADDR_ANY, 2566};
    fluent::Server server {address};

    server.onConnect([&](fluent::Context* context) {
        context->send("x");
    });

    server.ready();
    server.run();
    return 0;
}

// int main(void)
// {
//     int fd;

//     fd = socket(AF_INET, SOCK_STREAM, 0);

//     struct sockaddr_in servaddr;
//     memset(&servaddr, 0, sizeof servaddr);
//     servaddr.sin_family = AF_INET;
//     servaddr.sin_port = htons(9001);
//     servaddr.sin_addr.s_addr = 0;

//     const int on = 1;
//     setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);

//     bind(fd, (struct sockaddr *) &servaddr, sizeof servaddr);

//     listen(fd, 10);

//     int clifd = accept(fd, NULL, NULL);

//     sleep(5);

// //    char buf[1024];
// //    read(clifd, buf, sizeof buf);

//     close(clifd);

//     close(fd);

//     return 0;
// }