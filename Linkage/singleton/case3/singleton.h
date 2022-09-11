#pragma once

struct Singleton {
};

static Singleton& getInstance() {
    static Singleton s;
    return s;
}
