#### 主要流程

![lab2](C:\Users\aisha08\Desktop\lab2.png)

1. trace 系统调用

   需求知识：xv6 book 的第2章，4.3，4.4；

   ​					user/user.h，user/usys.pl；

   ​					kernel/syscall.h，kernel/syscall.c，kernel/proc.h，kernel/proc.c。

   该实验需要打印其他系统调用的信息。根据上面的分析和文档说明，首先需要给user.h、usys.pl（用来生成usys.S的辅助脚本）和syscall.h添加对应的函数的系统调用号，然后给syscall.c的系统调用数组添加对应的函数指针和函数头，在sysproc.c添加对应的函数实现，sysproc.c里主要是接收参数并给proc结构体复制，具体代码如下

   ```c
   uint64 sys_trace(void) {
       int mask;
       if (argint(0, &mask) < 0) 
           return -1;
       myproc()->mask = mask; 
       return 0;
   }
   ```

   这样看似完成了实验要求，但是实际功能并没有实现，真正打印其他系统调用信息的操作应该在syscall.c中进行，在syscall函数的末尾（其他系统调用结束后）输出信息：

   ```c
   // 要自己添加一个系统调用名称的字符串数组
   static char *syscall_name[] = {"", "fork", "exit", "wait", "pipe", "read", "kill",
                                  "exec", "fstat", "chdir", "dup", "getpid", "sbrk", "sleep",
                                  "uptime", "open", "write", "mknod", "unlink", "link",
                                  "mkdir", "close", "trace", "sysinfo"};
   
   void
   syscall(void)
   {
   	int num;
     	struct proc *p = myproc();
   
     	num = p->trapframe->a7;
     	if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
       	p->trapframe->a0 = syscalls[num]();
       	if(p->mask & (1 << num)){                          //从这里开始的后面三行为trace系统调用
         		printf("%d: syscall %s -> %d\n",
                		p->pid, syscall_name[num], p->trapframe->a0);
       	}
     	}
       else {
       	printf("%d %s: unknown sys call %d\n",
               	p->pid, p->name, num);
       	p->trapframe->a0 = -1;
     	}
   }
   ```

   最后要在proc.c的fork()函数里加上子进程复制父进程mask的代码：

   ```c
    np->mask = p->mask; // 加上这行，后面两行是fork()原本的
   
    np->sz = p->sz;
   
    np->parent = p;
   ```

   其他地方的添加按照实验文档来

   

2. sysinfo系统调用

   需求知识：xv6 book 的第2章，4.3，4.4；

   ​					user/user.h，user/usys.pl；

   ​					kernel/syscall.h，kernel/syscall.c，kernel/proc.h，kernel/proc.c;

   ​					kernel/sysinfo.h，vm.c里的copyout()	

   这里有几个难点，一个是进程，其实在/kernel/proc.c文件中一开始就定义了一个数组

   ```c
   struct proc proc[NPROC]
   ```

   这个数组就保存着所有的进程，所以只要遍历这个数组判断状态就好了，判断状态就是在proc结构体中的enum procstate state;

   ```c
   enum procstate { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };
   ```

   添加系统调用的过程和上一个人物类似，这里就不提了。具体sys_sysinfo函数的实现需要首先获得所需要的信息，然后获得传进系统调用函数的地址参数，将获得的信息复制到这个地址，由于是涉及内核态和用户态的地址转换，所以需要使用copyout函数：

   ```c
   uint64
   sys_sysinfo(void)
   {
     struct sysinfo info;
     uint64 addr;
     if(argaddr(0, &addr) < 0)
       return -1;
     info.freemem = freemem_size();
     info.nproc = proc_num();
     if(copyout(myproc()->pagetable, addr, (char*)&info, sizeof(info)) < 0)
       return -1;
     return 0;
   }
   ```

   关于freemem_size()函数，这里就需要理解kalloc.c的内容了，这个文件主要进行物理内存的管理，使用一个链表来管理空闲空间，而且一个链表节点就代表一页，所以遍历整个链表，节点数乘上内存页的大小就是空闲空间：

   ```c
   uint64 
   freemem_size(void)
   {
     struct run *r;
     uint64 num = 0;
     for (r = kmem.freelist; r; r = r->next){
       num++;
     }
     return num * PGSIZE;
   }
   ```

   关于proc_num()函数，需要理解proc.c的内容了，这个文件主要进行进程的管理，xv6用一个数组来维护所有的进程，不管是在运行的、在等待的还是没被分配的。所以nproc函数只需要遍历这个数组数清有多少没被分配的进程就行了：

   ```c
   int
   proc_num(void)
   {
     struct proc *p;
     int num = 0;
     for (p = proc; p < &proc[NPROC]; p++){
       acquire(&p->lock);
       if (p->state != UNUSED)
       {
         num++;
       }
       release(&p->lock);
     }
     return num;
   }
   ```

   记得在相应的地方加上函数声明

3. 总结

   这次实验难度不大，但是直接告诉了系统调用的原理，并不是简单的用户态向内核态转换这样一句话或者使用一个函数进行系统调用，同时也提及了一些物理内存和进程管理的内容，还对fork()函数中子进程对父进程的具体拷贝细节有了了解。虽然这个实验也就给了一个usys.pl代码，但确实给让我的总体的思路更清晰。

   **user/usys.pl**

   ```c
   sub entry {
       my $name = shift;
       print ".global $name\n";
       print "${name}:\n";
       print " li a7, SYS_${name}\n";
       print " ecall\n";
       print " ret\n";
   }
   ```

   这段代码仔细读读应该能了解大概意思，让我大概理清了过程，比如以trace为例：

   user/trace.c调用trace系统调用 -> markfile调用usys.pl代码通过汇编（a7）进入内核 -> 获得跟踪结果输出到屏幕。

