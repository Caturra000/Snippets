#include <sys/syscall.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

void check(int cond, const char *reason) {
    if(!cond) return;
    perror(reason);
    abort();
}

int main() {
    pid_t pid;
    printf("Enter a remote pid: ");
    scanf("%d", &pid);

    int pidfd = syscall(SYS_pidfd_open, pid, 0);
    check(pidfd < 0, "pidfd_open failed");

    int remote_fd = syscall(SYS_pidfd_getfd, pidfd, STDOUT_FILENO, 0);
    check(remote_fd < 0, "pidfd_getfd failed");

    const char buf[] = "jintianxiaomidaobilema";
    ssize_t written = write(remote_fd, buf, sizeof buf);
    check(written != sizeof buf, "write failed");

    printf("Done.\n");
    syscall(SYS_pidfd_send_signal, pidfd, SIGINT, NULL, 0);
    close(remote_fd);
    close(pidfd);
    return 0;
}
