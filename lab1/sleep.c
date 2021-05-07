#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(int argn, char* argv[])
{
    int num = atoi(argv[1]);
    if(argn != 2){
        printf("the argument must >= 2\n");
        exit(1);
    }

    printf("(nothing happens for a little while)\n");
    sleep(num);

    exit(0);
}