#include "kernel/types.h"
#include "user/user.h"

#define R 0
#define W 1

void transfer(int p[])
{
    int first = 0, n = 0;
    read(p[R], &first, sizeof(int));
    printf("prime %d\n", first);

    int flag = 0;
    int p_nextL[2];
    pipe(p_nextL);
    while(read(p[R], &n, sizeof(int)) == sizeof(int))
        if(n % first != 0)
            {
                write(p_nextL[W], &n, sizeof(int));
                flag = 1;
            }
    
    close(p[R]);
    p[R] = p_nextL[R], p[W] = p_nextL[W];
    close(p[W]);

    if(flag == 0)
    {
        close(p[R]);
        exit(0);
    }
}

int main(int argc, char *argv[])
{
    int p[2];
    pipe(p);

    for(int n = 2; n < 36; n++)
        write(p[W], &n, sizeof(int));
    close(p[W]);    
    
    for(int i = 0; i < 16; i++)//TODO:Hard Coding. Not sure about end condition.
    {
        if(fork() == 0)
            transfer(p);
        else
            wait((int *) 0);
    }
    exit(0);
}