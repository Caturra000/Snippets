// 关于各种形式的函数最直接的性能对比已经在function.cpp文件中给出数据了
// 这里不做比较

// ## 背景/动机
// 通常使用虚函数的时候会用到vector-of-pointer的做法
// 既你想用std::vector<Base*>来装入不同的多态子类，循环调用一个接口而不关心实现
// for(auto *ptr : vec) ptr->func()...
//
// ## 问题
// 但是从工程经验的角度来看，每个元素的局部性会很差，因为你通常你的多态实例是从堆上分配的
// （当然可以是栈上，但实际上很少这么干）
// 意味着很可能你的内存布局是支离破碎的，就是下一个元素对比上一个元素并不是彼此“近邻”的
// （实际上这取决于内存分配算法，但是大概率没有局部性的保证）
// 这种情况下，性能损耗的结论不能直接拿function.cpp里面的简单对比结果来套用
// 所以要测一下到底差多远
//
// ## 测试方案
// 简单点可以通过连续的数组配合足够跨缓存行的体积来模拟这种局部性不足的例子
//（这样子采样会相对准点，因为没有干多余的事情，不统计入堆分配的事件）
// 因为虚函数调用肯定需要访问实例下的vptr的值，既不开编译器优化的话，内存访问不可避免
//（这也是vcall行为上的特性）
// 所以我觉得这样子测试，正确性是ok的
//
// ## 得出数据
// 下面是一些perf数据
/*
 假设局部性良好
 Performance counter stats for './a.out':

         101255712      cache-misses                                                

      17.833244413 seconds time elapsed

      17.783121000 seconds user
       0.050008000 seconds sys

 假设局部性很差
 Performance counter stats for './a.out':

         604377815      cache-misses                                                

      34.119155924 seconds time elapsed

      33.979047000 seconds user
       0.140037000 seconds sys
*/
//
// ## 结论
// 在这种测试方案，数据规模相同的情况下
// cache miss会有6倍左右的差距，实际耗时翻倍
//
// 因此还是小心使用vector-of-pointer这种惯用写法
// 有可能是个坑
//
// ## 解决方案/建议
// 1. 不要这么写，谨记不作死就不会死
// 2. 买个超大缓存的CPU
//
// ## 更多
// cacheline.cpp还测试了并发下的cacheline共享问题
// 也可做点参考




#include <vector>
#include <cstddef>

// 修改这里来比较不同的case
constexpr bool enable_cache_miss_case = true;

using Cacheline_padding = char[enable_cache_miss_case ? 64 : 1];
constexpr size_t out_of_L3 = 10 * 1e6;
constexpr size_t loops = 1000;

struct Base {
    virtual void func() = 0;
};

struct Derived: Base {
    void func() override {}
private:
    Cacheline_padding _;
};

// Tooooo large, cannot be allocated on stack
Derived instances[out_of_L3];
Base* ptr_arr[out_of_L3];

int main() {
    for(size_t i = 0; i < out_of_L3; ++i) {
        ptr_arr[i] = &instances[i];
    }

    for(auto l = loops; l--;) {
        for(auto ptr : ptr_arr) {
            ptr->func();
        }
    }

    return 0;
}