#include <iostream>

// 使用case来完成duff's device
void private_memcpy(char *to, const char *from, size_t count) {
    size_t n = (count + 7) / 8;
    #define COPY *to = *from;
    #define ADVANCE to++, from++;
    switch (count % 8) {
    case 0: do { COPY ADVANCE
    case 7:      COPY ADVANCE
    case 6:      COPY ADVANCE
    case 5:      COPY ADVANCE
    case 4:      COPY ADVANCE
    case 3:      COPY ADVANCE
    case 2:      COPY ADVANCE
    case 1:      COPY ADVANCE
            } while (--n > 0);
    }
    #undef COPY
    #undef ADVANCE
}

int main() {
    const char from[] = "jintianxiaomidaobilema";
    char to[sizeof from];
    private_memcpy(to, from, sizeof from);
    std::cout << to << std::endl;
    return 0;
}
