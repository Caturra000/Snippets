#include "common.h"
#include <cstring>

int main() {
    output("main");
    auto pid = fork();
    if(!pid) {
        output("fork");
        // https://stackoverflow.com/questions/45430087/
        if(execv("nop", nullptr)) {
            std::cout << strerror(errno) << std::endl;
        }
        // hang();
    }
}
