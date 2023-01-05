#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char* 
filename(char *path) {
    char *p;
    for ( p = path + strlen(path); p >= path && *p != '/'; p-- );
    p++;
    return p;
}

void
find(char *path, char *name) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ( (fd = open(path, 0)) < 0 ) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if ( fstat(fd, &st) < 0 ) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type) {
        case T_DEVICE:
        case T_FILE:
            if ( strcmp(filename(path), name) == 0 ) {
                printf("%s\n", path);
            }
        break;
        case T_DIR:
            strcpy(buf, path);
            p = buf + strlen(buf);
            *p++ = '/';
            while( read(fd, &de, sizeof(de)) == sizeof(de) ) {
                if ( de.inum == 0 ) 
                    continue;
                if ( strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0 )
                    continue;
                memmove(p, de.name, strlen(de.name));
                p[strlen(de.name)] = 0;
                if ( stat(buf, &st) < 0 ) {
                    fprintf(2, "find: cannot stat %s\n", buf);
                    continue;
                }
                find(buf, name);
            }
        break;
    }
    close(fd);
}

int
main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(2, "Not enough arguments\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}