// 需要-std=c++17或以上版本才能编译

struct immovable {
    immovable() = default;
    immovable(immovable&&) = delete;
};

struct X: immovable {
    int v;
};

X test() {
    // return {1};
    return {{}, 1};
}

int main() {
    auto obj = test();
}
