#include <bits/stdc++.h>

int main() {
    // 输出：1
    // 也就是说ffs指的是最低位1
    // 而fls应该是相反的最高位1
    // 这个在看task state时有所帮助
    // https://github.com/Caturra000/RTFSC/blob/master/linux/proc/state/state.c
    std::cout << __builtin_ffs(0x801) << std::endl;
    return 0;
}