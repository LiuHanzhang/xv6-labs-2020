#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAX_LINE_LEN 1024

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(2, "error: missing argument\n");
        exit(1);
    }
    
    char *argv_exec[MAXARG];
    int idx;
    for(idx = 0; idx < argc - 1; idx++)
        argv_exec[idx] = argv[idx + 1];
    
    char line[MAX_LINE_LEN];
    if(read(0, line, MAX_LINE_LEN) < 0)
    {
        fprintf(2, "error: xargs failed to read from stdin\n");
        exit(1);
    }
    /*split the stdin line into argvs*/
    char *i, *j;
    for(i = line, j = line; *j != 0; j++)
        if(*j == ' ' || *j == '\n')
        {
            *j = 0;
            argv_exec[idx++] = i;
            i = j + 1;
        }
    argv_exec[idx] = i;
    argv_exec[idx + 1] = 0;

    if(fork() == 0)
        exec(argv_exec[0], argv_exec);
    else
        wait((int *) 0);
    
    exit(0);
}