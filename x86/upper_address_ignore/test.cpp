#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// 危险：高位Tagging (使用第58位)
void dangerous_high_bit_tagging() {
    // 分配内存
    int* ptr = new int(42);
    std::cout << "危险高位Tagging - 原始地址: " << ptr 
              << ", 值: " << *ptr << "\n";

    // 设置第58位为1 (从0开始计数)
    constexpr uintptr_t HIGH_BIT_58 = (1ULL << 58);
    uintptr_t tagged_ptr = reinterpret_cast<uintptr_t>(ptr) | HIGH_BIT_58;
    
    std::cout << "设置第58位的指针: " 
              << reinterpret_cast<void*>(tagged_ptr) << "\n";
    std::cout << "  原始地址的第58位: " 
              << ((reinterpret_cast<uintptr_t>(ptr) & HIGH_BIT_58 ? "1" : "0"))
              << "\n";
    std::cout << "  带Tag地址的第58位: " 
              << ((tagged_ptr & HIGH_BIT_58) ? "1" : "0") << "\n";

    // ==== 危险操作 ====
    std::cout << "尝试解引用高位Tagging指针...\n";
    int value = *reinterpret_cast<int*>(tagged_ptr); // 此处应崩溃!
    std::cout << "值: " << value << "\n"; // 这行不会执行

    delete ptr;
}

int main() {
    std::cout << "==== 运行高位Tagging测试 ====\n";
    dangerous_high_bit_tagging();
    return 0;
}
