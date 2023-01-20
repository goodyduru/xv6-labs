#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
getline(char *line[], int start) {
    int buf[1], n, count;
    char *arg, *p;
    count = start;
    arg = 0;
    p = arg;
    while ( (n = read(0, buf, 1)) > 0 && count < MAXARG ) {
        if ( buf[0] == '\n' ) {
            if ( arg != 0 ) {
                *p = '\0';
                line[count++] = arg;
            }
            return count;
        }

        if ( buf[0] == ' ' || buf[0] == '\t' ) {
            if ( arg != 0 ) {
                *p = '\0';
                line[count++] = arg;
                arg = 0;
                p = arg;
            }
            continue;
        }

        if ( arg == 0 ) {
            arg = malloc(100);
            p = arg;
        }
        *p++ = buf[0];
    }
    return count;
}

int
main(int argc, char *argv[]) {
    if ( argc < 2  ) {
        fprintf(2, "insufficient arguments\n");
    }
    char *arguments[MAXARG];
    int count, start;
    for ( int i = 1; i < argc; i++ ) {
        arguments[i-1] = argv[i];
    }
    start = argc - 1;
    while ( (count = getline(arguments, start)) > start ) {
        if ( fork() == 0 ) {
            exec(arguments[0], arguments);
        }
        else {
            wait(0);
            //free all other arguments memory
            for ( int i = start; i < count; i++ ) {
                free(arguments[i]);
            }
        }
    }
    exit(0);
}