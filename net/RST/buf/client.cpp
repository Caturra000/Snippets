#include <bits/stdc++.h>
#include "fluent.hpp"

// 验证socket缓冲区存在未read字节时关闭的行为
// http://blog.chinaunix.net/uid-10106787-id-3172066.html
// 这里提到可能发出RST
// 然而通过fluent测试并没有
//
// TODO 直接测他的代码吧
// update. 已测试，并没有RST

// 0: shutdown
// 1: close
int main(int argc, const char *argv[]) {

    bool forced = false;
    if(argc >= 2 && argv[1][0] == '1') {
        forced = true;
        std::cout << "enable forced mode" << std::endl;
    } else {
        std::cout << "disable forced mode" << std::endl;
    }

    fluent::InetAddress address {"127.0.0.1", 2566};
    fluent::Client client;

    client.connect(address)
    // 等待确实写入到socket buffer
    .wait(std::chrono::milliseconds(1000), [forced](fluent::Context *context) {
        if(forced) {
            ::close(context->socket.fd());
            context->socket.detach();
        } else {
            context->shutdown();
        }
    });

    client.run();
    return 0;
}

// int main(void)
// {
//     int fd, ret;

//     fd = socket(AF_INET, SOCK_STREAM, 0);

//     struct sockaddr_in servaddr;
//     memset(&servaddr, 0, sizeof servaddr);

//     servaddr.sin_family = AF_INET;
//     servaddr.sin_port = htons(9001);
//     servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

//     connect(fd, (sockaddr *) &servaddr, sizeof servaddr);

//     char buf[1024];

//     if (write(fd, buf, sizeof buf) != sizeof buf)
//         perror("write error");

//     if ((ret = read(fd, buf, sizeof buf)) < 0)
//         perror("read error");

//     fprintf(stderr, "Read %d Bytes\n", ret);

//     if (close(fd) < 0)
//         perror("close error");

//     return 0;
// }