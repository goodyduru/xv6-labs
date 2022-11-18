#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
print_prime(int parent_fds[]) {
    close(parent_fds[1]);
    int pid, fds[2], number, prime, size;
    prime = 0;
    while ( (size = read(parent_fds[0], &number, sizeof(number))) > 0 ) {
        if ( size != sizeof(number) ) {
            fprintf(2, "read error\n");
            exit(1);
        }
        if ( prime == 0 ) {
            prime = number;
            printf("prime %d\n", prime);
        }
        else if ( number % prime ) {
            pipe(fds);
            pid = fork();
            if ( pid == 0 ) {
                close(parent_fds[0]);
                print_prime(fds);
            }
            else {
                close(fds[0]);
                write(fds[1], &number, sizeof(number)); //write first number
                while ( read(parent_fds[0], &number, sizeof(number)) ) {
                    if ( number % prime == 0 ) continue;
                    write(fds[1], &number, sizeof(number));
                }
                close(fds[1]);
                close(parent_fds[0]);
                wait(0);
            }
        }
    }
    exit(0);
}

int
main() {
    int fds[2];
    pipe(fds);
    int pid = fork();
    if ( pid == 0 ) {
        print_prime(fds);
    }
    else {
        close(fds[0]);
        for ( int i = 2; i <= 35; i++ ) {
            write(fds[1], &i, sizeof(i));
        }
        close(fds[1]);
        wait(0);
    }
    exit(0);
}