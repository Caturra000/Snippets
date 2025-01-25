#include <stdexec/execution.hpp>

int my_func(int i) { return i; }

int main() {
    auto s = stdexec::just(6) | stdexec::then(my_func);
    stdexec::sync_wait(s);
    return 0;
}