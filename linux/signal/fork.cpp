#include "common.h"

int main() {
    output("main");
    auto pid = fork();
    if(!pid) {
        output("child");
    }
}
