1. #### Uthread：switching between threads

   1. ```markdown
      要求：创建线程、存储/恢复寄存器来进行多线程切换
      ```

   2. ```c
      // 1. 主要是实现从一个线程切换到另一个线程时，线程间寄存器信息的交换
      // 2. 线程的寄存器信息就是在kernel/proc.h里的context结构体，因此需要引入相关头文件，具体实现如下
      
      // 头文件
      #include "kernel/riscv.h"
      #include "kernel/spinlock.h"
      #include "kernel/param.h"
      #include "kernel/proc.h"
      
      struct thread {
        char       stack[STACK_SIZE]; /* the thread's stack */
        int        state;             /* FREE, RUNNING, RUNNABLE */
        struct context threadContext; /* register massages */ // 添加context信息
      };
      extern void thread_switch(struct context* t, struct context* current_thread); // 修改参数
      
      void 
      thread_schedule(void)
      {
          /* YOUR CODE HERE
           * Invoke thread_switch to switch from t to next_thread:
           * thread_switch(??, ??);
           */
          thread_switch(&t->threadContext, &current_thread->threadContext); // 添加线程交换
      }
      
      void 
      thread_create(void (*func)())
      {
        // YOUR CODE HERE
        t->threadContext.ra = (uint64)func;                     // init
        t->threadContext.sp = (uint64)(t->stack) + STACK_SIZE;  // top of thread's kernel stack
      }
      
      // 修改user/uthread_switch.S，实现保存/恢复寄存器
      	.text
      
      	/*
               * save the old thread's registers,
               * restore the new thread's registers.
               */
      
      	.globl thread_switch
      thread_switch:
      	/* YOUR CODE HERE */
      	sd ra, 0(a0)
      	sd sp, 8(a0)
      
      	sd s0, 16(a0)
      	sd s1, 24(a0)
      	sd s2, 32(a0)
      	sd s3, 40(a0)
      	sd s4, 48(a0)
      	sd s5, 56(a0)
      	sd s6, 64(a0)
      	sd s7, 72(a0)
      	sd s8, 80(a0)
      	sd s9, 88(a0)
      	sd s10, 96(a0)
      	sd s11, 104(a0)
      
      	ld ra, 0(a1)
      	ld sp, 8(a1)
      
      	ld s0, 16(a1)
      	ld s1, 24(a1)
      	ld s2, 32(a1)
      	ld s3, 40(a1)
      	ld s4, 48(a1)
      	ld s5, 56(a1)
      	ld s6, 64(a1)
      	ld s7, 72(a1)
      	ld s8, 80(a1)
      	ld s9, 88(a1)
      	ld s10, 96(a1)
      	ld s11, 104(a1)
      
      	ret    /* return to ra */
      ```

2. #### Using threads 和 Barrier 的实现请看代码 notxv6 下的ph.c 和 barrier.c，主要涉及线程，锁和条件变量等