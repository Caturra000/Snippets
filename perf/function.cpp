// 修改自：https://blog.csdn.net/luchengtao11/article/details/120069120
// - 避免O3下忽略调用的问题
// - 传参类型保持一致
// - 添加直接的函数调用
// - 添加单次调用的成本计算
// - 原测试缺少说服力，尤其std::bind大概率没有inline处理

// 结论：
// - std::bind和ptr是直接调用函数和lambda的2-3倍差距（O3）
// - 普通函数的调用大概是0.4-2ns水平，还得看CPU水平
// - 但是就整体开销来看，其实用哪一种都不必太在意
// - （原作者测试结果）如果不使用inline优化，那可能会导致数量级的性能差距

// TODO 添加virtual，使用std::function（不含std::bind）或者custom class

/*
[device] Ryzen7 5800H
===total(ms)===
lambda: 41
bind: 113
func_ptr: 116
bind_lambda: 144
func: 43
===average(ns)===
lambda: 0.41
bind: 1.13
func_ptr: 1.16
bind_lambda: 1.44
func: 0.43


[device] godbolt online
===total(ms)===
lambda: 242
bind: 253
func_ptr: 675
bind_lambda: 161
func: 229
===average(ns)===
lambda: 2.42
bind: 2.53
func_ptr: 6.75
bind_lambda: 1.61
func: 2.29
*/

#include <iostream>
#include <chrono>
#include <functional>

auto res = 0;

int test_func(int i)
{
   return i;
 }

 auto test_lambda = [](int i)
 {
    return i;
 };

 auto test_bind = std::bind(test_func, std::placeholders::_1);

 auto test_func_ptr = test_func;

 std::function<void(int)> test_bind_lambda = test_lambda;
 int main()
 {
    long times = 1e8;

    auto t0 = std::chrono::high_resolution_clock::now();
    // 使用volatile避免lambda直接被优化忽略
    for (volatile auto i = 0; i < times; i++)
        test_lambda(i);
    auto t1 = std::chrono::high_resolution_clock::now();


    for (volatile auto i = 0; i < times; i++)
        test_bind(i);
    auto t2 = std::chrono::high_resolution_clock::now();


    for (volatile auto i = 0; i < times; i++)
        test_func_ptr(i);
    auto t3 = std::chrono::high_resolution_clock::now();
    for (volatile auto i = 0; i < times; i++)
        test_bind_lambda(i);
    auto t4 = std::chrono::high_resolution_clock::now();
    for (volatile auto i = 0; i < times; i++)
        test_func(i);
    auto t5 = std::chrono::high_resolution_clock::now();

    using TimePoint = decltype(t1);
    auto cal = [](TimePoint end, TimePoint start) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    };

    std::cout << "===total(ms)===" << std::endl;
    std::cout << "lambda: " << cal(t1, t0).count() << std::endl;
    std::cout << "bind: " << cal(t2, t1).count() << std::endl;
    std::cout << "func_ptr: " << cal(t3, t2).count() << std::endl;
    std::cout << "bind_lambda: " << cal(t4, t3).count() << std::endl;
    std::cout << "func: " << cal(t5, t4).count() << std::endl;

    constexpr auto _1ms = std::chrono::milliseconds(1);
    constexpr auto unit = std::chrono::duration<double, std::nano>(_1ms).count();

    std::cout << "===average(ns)===" << std::endl;
    std::cout << "lambda: " << unit * cal(t1, t0).count() / times << std::endl;
    std::cout << "bind: " << unit * cal(t2, t1).count() / times << std::endl;
    std::cout << "func_ptr: " << unit * cal(t3, t2).count() / times << std::endl;
    std::cout << "bind_lambda: " << unit * cal(t4, t3).count() / times << std::endl;
    std::cout << "func: " << unit * cal(t5, t4).count() / times << std::endl;

    return 0;
}
