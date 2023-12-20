#include <coroutine>
#include <iostream>
#include <stdexcept>
#include <thread>

// 改编自：https://en.cppreference.com/w/cpp/language/coroutines
// 这个程序增加了sleep操作，使得jthread先执行resume
// 另外我特意使用了不安全的访问方式（见p_out和out的对比）
// 理论上不能正常运行，如果能，那只是运气好
// 使用-fsanitize=address编译选项就能暴露问题（use-after-free）

// 可以更改配置为true对比情况
constexpr bool run_safe = false;

auto switch_to_new_thread(std::jthread& out)
{
    struct awaitable
    {
        std::jthread* p_out;
        bool await_ready() { return false; }
        void await_suspend(std::coroutine_handle<> h)
        {
            std::jthread& out = *p_out;
            if (out.joinable())
                throw std::runtime_error("Output jthread parameter not empty");
            out = std::jthread([h] { h.resume(); });
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(+1s);
            if constexpr (run_safe) {
                // This is OK
                std::cout << "New thread ID: " << out.get_id() << '\n';
            } else {
                // Potential undefined behavior: accessing potentially destroyed *this
                std::cout << "New thread ID: " << p_out->get_id() << '\n';
            }
        }
        void await_resume() {
            std::cout << "resume!!!" << std::endl;
        }
    };
    return awaitable{&out};
}

struct task
{
    struct promise_type
    {
        task get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
};

task resuming_on_new_thread(std::jthread& out)
{
    std::cout << "Coroutine started on thread: " << std::this_thread::get_id() << '\n';
    co_await switch_to_new_thread(out);
    // awaiter destroyed here
    std::cout << "Coroutine resumed on thread: " << std::this_thread::get_id() << '\n';
}

int main()
{
    std::jthread out;
    resuming_on_new_thread(out);
}
