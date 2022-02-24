#include <iostream>
#include <boost/asio.hpp>

int main() {
    boost::asio::io_context context;
    using namespace std::chrono_literals;
    boost::asio::steady_timer timer(context, 5s);
    timer.async_wait([](const boost::system::error_code &) {
        std::cout << "hello world" << std::endl;
    });
    context.run();
    return 0;
}