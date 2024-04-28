#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
// #include <unistd.h>

int main(int argc, char *argv[]) {
    int parent_to_child[2]; // 0 is read, 1 is write
    int child_to_parent[2];

    // create pipes
    if (pipe(parent_to_child) == -1 || pipe(child_to_parent) == -1) {
        return 1;
    }

    int pid = fork();
    if (pid < 0) { // fork failed
        exit(1);
    } else if (pid == 0) { // child
        close(parent_to_child[1]);
        close(child_to_parent[0]);

        char idk = 'B';
        read(parent_to_child[0], &idk, sizeof(idk));
        printf("%d: received ping\n", getpid());
        write(child_to_parent[1], &idk, sizeof(idk));

        close(parent_to_child[0]);
        close(child_to_parent[1]);
        exit(0);
    } else { // parent
        close(parent_to_child[0]);
        close(child_to_parent[1]);

        char mybyte = 'A';

        write(parent_to_child[1], &mybyte, sizeof(mybyte));
        read(child_to_parent[0], &mybyte, sizeof(mybyte));
        printf("%d: received pong\n", getpid());

        close(parent_to_child[1]);
        close(child_to_parent[0]);
        exit(0);
    }
}