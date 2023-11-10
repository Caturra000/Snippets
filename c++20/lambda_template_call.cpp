#include <thread>
int main() {
    auto lambda = []<typename T>{};

    // 你以为这样就行了？NO
    // lambda<int>();

    // 要这样
    lambda.template operator()<int>();

    // 但我还不知道该怎么丢到std::thread里面
    std::thread {/*???*/};
}
