#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main() {
    int fds[2], pid, n, current_pid;
    char byte[1];
    pipe(fds);
    pid = fork();
    if ( pid == 0 ) {
        if ( (n = read(fds[0], byte, sizeof(byte))) > 0 ) {
            current_pid = getpid();
            printf("%d: received ping\n", current_pid);
            if ( write(fds[1], byte, sizeof(byte)) != n ) {
                fprintf(2, "child: write error\n");
                exit(1);
            }
            exit(0);
        }
        else {
            fprintf(2, "child: read error\n");
            exit(1);
        }
    }
    else {
        byte[0] = 'g';
        n = write(fds[1], byte, sizeof(byte));
        current_pid = getpid();
        if ( n != 1 ) {
            fprintf(2, "parent: write error\n");
            exit(1);
        }
        wait(0);
        if ( read(fds[0], byte, sizeof(byte)) != n ) {
            fprintf(2, "parent: read error\n");
            exit(1);
        }
        printf("%d: received pong\n", current_pid);
    }
    exit(0);
}