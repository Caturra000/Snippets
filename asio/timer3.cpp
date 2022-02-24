#include <iostream>
#include <boost/asio.hpp>

int main() {
    boost::asio::io_context context;
    using namespace std::chrono_literals;
    boost::asio::steady_timer timer(context, 1s);
    int count = 0;
    using Function = std::function<void(const boost::system::error_code &)>;
    Function callback = [&](const boost::system::error_code &) {
        if(count < 5) {
            std::cout << count << std::endl;
            count++;
            timer.expires_at(timer.expiry() + 1s);
            timer.async_wait(callback);
        }
    };
    timer.async_wait(callback);
    context.run();
    std::cout << "final count: " << count << std::endl;
    return 0;
}