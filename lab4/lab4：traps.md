预备知识：

- 阅读xv6 book章节4；
- 从user space过渡到kernel space，kernel space返回到user space的汇编代码：kernel/trampoline.S；
- 解决所有中断的代码：kernel/trap.c。

assembly 任务查看寄存器汇编代码以及 GDB 调试。

1. #### backtrace

   实验需求：

   ```markdown
   在kernel/printf.c中实现backtrace()函数。在sys_sleep()中插入对这个函数的调用，然后运行bttest，它调用sys_sleep。你的输出应该是：
   backtrace:
   0x0000000080002cda
   0x0000000080002bb6
   0x0000000080002898
   在bttest之后退出qemu。在你的terminal中：地址可能是不同的，但如果你运行addr2line -e kernel/kernel（riscv64-unknown-elf-addr2line -e kernel/kernel）并且把上面地址复制粘贴如下所示：
   $ addr2line -e kernel/kernel
   0x0000000080002de2
   0x0000000080002f4a
   0x0000000080002bfc
   Ctrl-D
   你应该可以看到像下面所示：
   kernel/sysproc.c:74
   kernel/syscall.c:224
   kernel/trap.c:85
   ```

   按提示实现：

   1. 在 kernel/defs.h 中声明 backtrace()；

   2. 在 kernel/riscv.h 中增加r_fp()的实现，用来获得当前函数的栈指针，存放于寄存器s0，实验文档提供的代码：

      ```c
      static inline uint64
      r_fp()
      {
        uint64 x;
        asm volatile("mv %0, s0" : "=r" (x) );
        return x;
      }
      ```

   3. 在 kernel/printf.c 中新增 backtrace() 的实现

      ```c
      void
      backtrace()
      {
        uint64 fp = r_fp();
        uint64 bottom = PGROUNDUP(fp); //栈底地址
        uint64 r_addr;
        printf("backtrace:\n");
        while (fp < bottom)
        {
          r_addr = *(uint64 *)(fp - 8);
          printf("%p\n", r_addr);
          fp = *(uint64 *)(fp - 16);
        }
      }
      ```

2. #### alarm

   实验需求：

   ```markdown
   在这个练习中，你将为xv6添加一个特性：
   在进程使用cpu时间时，定期发出警报。对于compute-bound进程（想限制它们使用多少cpu时间），或对于既想计算也想采取一些周期性动作的进程，这可能是有用的。
   更普遍一些，你将能实现一个原始形式的用户级中断/fault handlers；你可以使用某些相似的东西来处理在应用中的page faults。
   如果通过alarmtest和usertests，你的解决方案就是正确的。
   你应该添加一个新的sigalarm(interval, handler)system call。如果一个应用调用sigalarm(n, fn)，那么在每n ticks个cpu time（程序花费）后，kernel应该导致应用函数fn被调用。
   当fn返回时，应用应该从它离开的地方重新恢复。在xv6中一个trick是一个相当任意的时间单位，取决于硬件定时器多久生成一个中断。
   如果一个应用调用sigalarm(0,0)，kernel应该停止生成周期alarm call。
   你将找到一个文件user/alarmtest.c在xv6代码库。把它添加到Makefile。它将不会编译成功，直到你添加sigalarm和sigreturn system calls。
   alarmtest在test0中调用sigalarm(2, periodic)，来告知kernel每两个tick强制调用periodic()，然后自旋一会。
   你可以在user/alarmtest.asm中看到allarmtest的汇编代码，可以用来调试。当alarmtest生成下面输出并且usertests正确运行，你的方案就是正确的。
   ```

   1. 与 lab1 实验类似，在 user/user.h 中的声明 sigalarm() 与 sigreturn()，然后更新 user/usys.pl（可以生成user/usys.S），kernel/syscall.h，和 kernel/syscall.c 来允许 alarmtest 调用调用 sigalarm 和 sigreturn system calls。

   2. 在 kernel/proc.h 中的 proc 结构体中添加新的成员；

      ```c
      struct proc {
        struct spinlock lock;
        ...
        uint64 total_ticks;          // 从上一次调用开始走过的时间
        uint64 alarm_interval;       // alarm 时间
        void (*handler)();           // alarm Func
        struct trapframe savetrapframe; // 存储时钟中断前的寄存器信息，注意不要声明为指针形式 ---- test 1,2 需要
        uint64 flag;                    // 判断是否还在 alarm 中断中 ---- test 1,2 需要
      };
      ```

   3. 在 kernel/sysproc.c 中增加 sys_sigalarm() 与 sys_sigreturn()系统调用；

      ```c
      uint64
      sys_sigalarm(void)
      {
        struct proc *p = myproc();
        int interval;
        uint64 handler;
        if(argint(0, &interval) < 0)
          return -1;
        if(argaddr(1, &handler) < 0)
          return -1;
        p->alarm_interval = interval;
        p->handler = (void (*)())handler;
        p->flag = 0;		// test 1,2 需要
        return 0;
      }
      
      uint64
      sys_sigreturn(void)
      {
        struct proc *p = myproc();		// test 1,2 需要
        *p->trapframe = p->savetrapframe;	// test 1,2 需要
        p->flag = 0;						// test 1,2 需要
        return 0;
      }
      ```

   4. 在 kernel/trap.c 中的 usertrap() 中添加；

      ```c
      void
      usertrap(void)
      {
        ...
        if(p->killed)
          exit(-1);
      
        // give up the CPU if this is a timer interrupt.
        if(which_dev == 2){
          p->total_ticks++;								// +
          if(p->total_ticks == p->alarm_interval){		// +
            if(!p->flag){									// +  test 1,2  需要
              p->savetrapframe = *p->trapframe;			// +  test 1,2  需要
              p->flag = 1;								// +  test 1,2  需要
              p->trapframe->epc = (uint64)p->handler;		// +   
            }												// + 
            p->total_ticks = 0;							// +  
          }												// +
          yield();
        }
        ...
      }
      ```

   5. 注意需要在 kernel/proc.c 中的 allocproc() 初始化 total_ticks;

      ```c
      found:
        p->pid = allocpid();
        p->total_ticks = 0; // +
      ```

3. 实验结果

   ![img](file:///C:\Users\aisha08\AppData\Roaming\Tencent\Users\1429402802\QQ\WinTemp\RichOle\BF4_5P~521NA3AKS]7XSJ_R.png)

总结一下：lab4 总体来说就是加深对用户态切换到内核态这个过程的理解，总体来说不算太难。

