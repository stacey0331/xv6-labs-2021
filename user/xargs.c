#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include <kernel/param.h>

#include "user/user.h"

#define ARG_LEN 1024

// for echo hello too | xargs echo bye, argv[] = xargs, echo, bye
int main(int argc, char * argv[]) {
    char ** argv2 = malloc(sizeof(char*) * MAXARG);
    int i = 0;
    for (; i < (argc - 1); i++) {
        argv2[i] = argv[i+1];
    }

    char curr;
    char * line = malloc(sizeof(char) * ARG_LEN);
    int lineidx = 0;
    while (read(0, &curr, 1) > 0) {
        if (curr == '\n') {
            line[lineidx] = '\0';
            argv2[i] = line;
            i++;
            lineidx = 0;
        } else {
            line[lineidx] = curr;
            lineidx += 1;
        }
    }
    argv2[i] = '\0';
    free(line);

    int pid = fork();
    if (pid < 0) { // fork failed
        printf("fork\n");
        return -2;
    } else if (pid == 0) { // child
        exec(argv2[0], &argv2[0]);
        printf("execvp");
        exit(1);
    } else {
        wait(0);
        free(argv2);
        exit(0);
    }
    return 0;
}