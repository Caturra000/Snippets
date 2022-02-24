#include <iostream>
#include <boost/asio.hpp>

int main() {
    boost::asio::io_context context;
    using namespace std::chrono_literals;
    // 为什么一个timer需要context作为传参，起到什么作用？
    // 看源码跟到这一块
    // impl_.get_service().expires_at(impl_.get_implementation(), expiry_time, ec);
    // 其中impl是一个io_object_impl
    boost::asio::steady_timer timer(context, 5s);
    timer.wait();
    std::cout << "hello world" << std::endl;
    return 0;
}