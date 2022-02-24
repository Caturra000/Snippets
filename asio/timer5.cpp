#include <iostream>
#include <vector>
#include <boost/asio.hpp>

class Printer {
public:
    Printer(boost::asio::io_context &context)
        : _context(context),
          _strand(boost::asio::make_strand(context)),
          _timer1(context, std::chrono::seconds(1)),
          _timer2(context, std::chrono::seconds(1)) {
        std::vector<boost::asio::steady_timer *> timers { &_timer1, &_timer2 };
        for(auto pTimer : timers) {
            auto &timer = *pTimer;
            timer.async_wait(boost::asio::bind_executor(_strand, [&] {
                print(timer);
            }));
        }
    }

    void print(boost::asio::steady_timer &timer) {
        using namespace std::chrono_literals;
        if(_count < 5) {
            std::cout << _count << std::endl;
            _count++;
            timer.expires_at(timer.expiry() + 1s);
            timer.async_wait(boost::asio::bind_executor(_strand, [&] {
                print(timer);
            }));
        }
    }


    boost::asio::io_context &_context;
    int _count {};
    boost::asio::strand<boost::asio::io_context::executor_type> _strand;
    boost::asio::steady_timer _timer1;
    boost::asio::steady_timer _timer2;
};

int main() {
    boost::asio::io_context context;
    Printer printer(context);
    auto run = [&] { context.run(); };
    std::thread t {run};
    run();
    t.join();
    return 0;
}