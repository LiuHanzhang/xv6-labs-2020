#include "kernel/types.h"
#include "user/user.h"

#define R 0
#define W 1

int main(int argc, char *argv[])
{
    int p[2];
    pipe(p);

    if(fork() == 0)
    {
        char byte = 0;
        read(p[R], &byte, 1);
        int pid = getpid();
        printf("%d: received ping\n", pid);
        write(p[W], &byte, 1);
    }
    else
    {
        char byte = 1;
        write(p[W], &byte, 1);
        wait((int *)0);
        read(p[R], &byte, 1);
        int pid = getpid();
        printf("%d: received pong\n", pid);
    }
    exit(0);
}