#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define N 5
char buf[N];

void
pong(int *parent_to_child, int *child_to_parent) {
  if (read(parent_to_child[0], buf, N) < 0) {
    printf("read failed\n");
  }
  printf("%d: received %s\n", getpid(), buf);
  if (write(child_to_parent[1], "pong", 4) != 4) {
    printf("write failed\n");
  }
}

void
ping(int *parent_to_child, int *child_to_parent) {
  
  if (write(parent_to_child[1], "ping", 4) != 4) {
    printf("write failed\n");
  }
  if (read(child_to_parent[0], buf, N) < 0) {
    printf("read failed\n");
  }
  printf("%d: received %s\n", getpid(), buf);
}

int
main(int argc, char *argv[])
{
  int parent_to_child[2];
  int child_to_parent[2];

  int pid;

  if (pipe(parent_to_child) < 0 || pipe(child_to_parent) < 0) {
    printf("pipe failed\n");
  }
  if ((pid = fork()) < 0) {
    printf("fork failed\n");
  }
  if (pid == 0) {
    pong(parent_to_child, child_to_parent);
  } else {
    ping(parent_to_child, child_to_parent);
  }
  
  exit(0);
}
int
mine() {
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
