#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[])
{
    int i, j = 0, ret, l = 0, m = 0;
    char block[32]; // 读缓冲区
    char buf[32];   //存储每一行 \n
    char *p = buf;
    char *lineSplit[32]; //存储参数

    // 存参数
    for(i = 1; i < argc; i++){
        lineSplit[j++] = argv[i];
    }

    // 把键盘输入读入 block 中
    while((ret = read(0, block, sizeof(block))) > 0){
        for(l = 0; l < ret; l++){
            if(block[l] == '\n'){
                buf[m] = 0;
                m = 0;
                // 24 - 28 行反复向 argv[] 中添加每一行的参数
                lineSplit[j++] = p;
                p = buf;
                lineSplit[j] = 0;
                j = argc - 1;

                // 子进程重定向
                if(fork() == 0){
                    exec(argv[1], lineSplit);
                }
                wait(0); 
            }
            else if(block[l] == ' '){
                buf[m++] = 0;
                lineSplit[j++] = p;
                p = &buf[m];
            }
            else{
                buf[m++] = block[l];
            }
        }
    }

    exit(0);
}