#include <vector>
#include <iostream>

// 简单的证明for-each中使用auto&&的存在价值

int main() {
    std::vector<bool> vec(sizeof "尝试用经典巨坑vector<bool>说明问题", !!"元素全部为true");

    // 由于返回的是“代理类”，因此不能用auto&进行for-each操作，它不是左值，连编译都做不到
    // 注：代理类是一个_Bit_reference
    // for(auto &v : vec);

    // 这是允许的，因为const&左右值都能绑定
    // 但是第二个问题在于const语义是read only，没法做出修改
    for(const auto &v : vec);

    // auto&&就解决了这两个问题
    for(auto &&v : vec) {
        v = false;
    }

    for(auto v : vec) {
        std::cout << v;
    }
    std::cout << std::endl;
}