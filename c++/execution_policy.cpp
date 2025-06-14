#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>
#include <execution>

// 修改自cppreference
// https://en.cppreference.com/w/cpp/algorithm/execution_policy_tag.html
// 注意execution policy可能存在问题
// 它可能会一声不吭地把没有链接tbb的情况直接fallback到单线程实现
// （在部分配置环境中会抛出链接错误，但是没有绝对的保证）

namespace execution = std::execution;
 
void measure([[maybe_unused]] auto policy, std::vector<std::uint64_t> v)
{
    const auto start = std::chrono::steady_clock::now();
    std::sort(policy, v.begin(), v.end());
    const auto finish = std::chrono::steady_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(finish - start)
              << '\n';
};
 
int main()
{
    std::vector<std::uint64_t> v(1'000'000);
    std::mt19937 gen {std::random_device{}()};
    std::ranges::generate(v, gen);
 
    measure(execution::seq, v);
    measure(execution::unseq, v);
    measure(execution::par_unseq, v);
    measure(execution::par, v);
}
