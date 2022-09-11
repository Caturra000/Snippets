#pragma once

struct Singleton {
    // 这里默认是按照static inline来处理的
    // 尝试构造为真·static
    // （话说为啥我要搞这么复杂？简化版见case3）
    __attribute__((visibility("hidden")))
    static Singleton& getInstance() {
        static Singleton instance;
        return instance;
    }
};
