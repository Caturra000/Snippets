#pragma once

struct Singleton {
    static Singleton& getInstance() {
        static Singleton instance;
        return instance;
    }
};
