#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    pid_t pid = getpid();
    if(pid < 0) {
        perror("getpid failed");
        abort();
    }
    printf("Start: %d\n", pid);
    pause();
}
