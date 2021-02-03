#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[])
{
    if(argc == 1)
        {
            write(2, "error: missing argument\n", 24);
            exit(1);
        }
    
    int sleep_sec = atoi(argv[1]);
    sleep(sleep_sec);
    exit(0);
}