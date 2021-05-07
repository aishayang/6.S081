#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// child:pid ping 
// parent: pid pong 
int main()
{
    int p[2];
    pipe(p);

    char buf[] = {'X'};
    int len = sizeof(buf);

    int pid = fork();

    // 子进程
    if(pid == 0) {
        int ret = read(p[0], buf, len);
        if(ret == len){
            printf("%d: received ping\n", getpid());
        }
        else{
            printf("child read error\n");
            exit(1);
        }
        close(p[0]);
        ret = write(p[1], buf, len);
        if(ret != len){
            printf("child write error\n");
            exit(1);
        }
        close(p[1]);
        exit(0);
    }
    // 父进程
    else if(pid > 0){
        write(p[1], buf, len);
        close(p[1]);
        
        wait(0);
        int ret = read(p[0], buf, len);
        if(ret == len){
            printf("%d: received pong\n", getpid());
        }
        else{
            printf("parent read error\n");
            exit(1);
        }
        close(p[0]);
        exit(0);
    }
    else{
        printf("fork error\n");
        exit(1);
    }

    exit(0);
}