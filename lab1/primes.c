#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void func(int* input, int num){
    if(num == 1){
        printf("prime %d\n", *input);
        return;
    }

    int p[2], i;
    int prime = *input;
    int tmp;
    // 输出管道里的第一个素数
    printf("prime %d\n", prime);
    pipe(p);


    // 向管道里写入下一轮数字
    if(fork() == 0){
        for (i = 0; i < num; i++){
            tmp = *(input + i);
            write(p[1], (char *)(&tmp), 4);
        }
        exit(0);
    }
    close(p[1]);

    // 从管道中读取每一个数，筛出非素数
    if(fork() == 0){
        int count = 0;
        char buf[4];
        while(read(p[0], buf, 4) != 0){
            tmp = *((int *)buf);
            // 与当前管道的第一个数整除
            if(tmp % prime != 0){
                *input = tmp;
                input += 1;
                count++;
            }
        }
        func(input - count, count);
        exit(0);
    }

    // 回收子进程
    wait(0);
    wait(0);
}

int main()
{
    int input[34], i;

    for (i = 0; i < 34; i++){
        input[i] = i + 2;
        // printf("%d ", input[i]);
    }

    func(input, 34);

    exit(0);
}