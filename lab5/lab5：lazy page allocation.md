预备知识：

- XV6 book 4.6	
- kernel/trap.c、kernel/vm.c、kernel/sysproc.c

1. #### Eliminate allocation from sbrk()

   任务要求：

   ```markdown
   删除 sbrk() 系统调用中的页面分配，原本 sbrk() 会给进程分配 n 字节的物理内存空间，然后返回新分配区域的开始（p->sz+n）；现在要求只增加 p->sz（p->sz+n），但是返回的是 old size，它应该不分配内存——所以你该删除对 growproc() 的调用（但你仍然要增加进程尺寸）。
   ```

   实现：

   ```c
   uint64
   sys_sbrk(void)
   {
     int addr;
     int n;
   
     if(argint(0, &n) < 0)
       return -1;
     addr = myproc()->sz;
     myproc()->sz += n;		// +
     // if(growproc(n) < 0)
     //   return -1;
     return addr;
   }
   ```

2. #### Lazy allocation

   任务要求：

   ```markdown
   修改 trap.c 中的代码对来自用户空间的 page fault（在 faulting address 映射一个新分配的物理内存页）做出响应。
   然后返回到用户空间来让进程继续执行。
   你应该在 printf（生成”usertrap():...”信息）之前添加你的代码。
   更改可以使 echo hi 正常需要的任何其他xv6 kernel代码。
   ```

   实现：

   ```c
   // 1. 修改kernel/trap.c的usertrap()，使得在发生fault page时，给虚拟地址分配物理页 
   // 2. 修改kernel/vm.c的uvmunmap()，让进程销毁时，对于尚未分配实际物理页的虚拟地址，不做处理直接跳过
   void
   usertrap(void)
   {
     ...
     } else if((which_dev = devintr()) != 0){
       // ok
     }else if(r_scause() == 13 || r_scause() == 15){			// +
       uint64 stval = r_stval();
       char *page = kalloc();
       if(page == 0)
         p->killed = 1;
       memset(page, 0, PGSIZE);
       if(mappages(p->pagetable, PGROUNDDOWN(stval), PGSIZE, 
                   (uint64)page, PTE_W | PTE_X | PTE_R | PTE_U) != 0)
         p->killed = 1;
     }															// +
     else {
       printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
       printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
     ...
   }
   
   void
   uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
   {
    	...
       if((*pte & PTE_V) == 0)				
         // panic("uvmunmap: not mapped");		// 任务一报 panic 处
         continue;								// +
   	...
   }
   ```

3. #### Lazytests and Usertests

   任务要求：

   ```markdown
   提供两个测试函数 lazytests() 和 usertests(), 修改代码通过这两个函数的测试。
   1.  处理sbrk()负参数
   2.	如果page-faults的虚拟内存地址比sbrk()分配的大，则杀掉此进程
   3.	正确处理fork() parent-to-child 内存拷贝
   4.	处理这么一种情况：进程传递一个来自sbrk()的有效地址给system call例如read or write，但是那些地址的内存尚未分配
   5.	正确处理超出内存：如果kalloc()在page fault handler中失败，杀掉当前进程
   6.	处理user stack下的invalid page fault
   ```

   实现：

   1. 修改 kernel/sysproc.c 的 sys_sbrk() 。

      ```c
      // 注意 uvmunmap()的参数，将 addr 和 new_sz 设置为 uint64 类型
      uint64
      sys_sbrk(void)
      {
        uint64 addr;
        int n;
        uint64 new_sz;
      
        if(argint(0, &n) < 0)
          return -1;
        addr = myproc()->sz;
        new_sz = addr + n;
        if(new_sz >= MAXVA)
          return addr;
        if(n < 0)
        {
          if(new_sz > addr){
            new_sz = 0;
            uvmunmap(myproc()->pagetable, 0, PGROUNDUP(addr) / PGSIZE, 1);
          }
          else{
            uvmunmap(myproc()->pagetable, PGROUNDUP(new_sz), 
                     (PGROUNDUP(addr) - PGROUNDUP(new_sz)) / PGSIZE, 1);
          }
        }
        myproc()->sz = new_sz;
        return addr;
      }
      ```

   2. 修改 kernel/trap.c 中的 usertrap() 。

      ```c
      void
      usertrap(void)
      {
        ...
        } else if((which_dev = devintr()) != 0){
          // ok
        }else if(r_scause() == 13 || r_scause() == 15){		// +
          uint64 stval = r_stval();
          if(stval >= p->sz){
            p->killed = 1;
          }
          else{
            uint64 protectTop = PGROUNDDOWN(p->trapframe->sp);
            uint64 stvalTop = PGROUNDUP(stval);
            if(protectTop != stvalTop){
              char *page = kalloc();
              if(page == 0)
                p->killed = 1;
              else{
                memset(page, 0, PGSIZE);
                if(mappages(p->pagetable, PGROUNDDOWN(stval), PGSIZE, (uint64)page, PTE_W | PTE_X | PTE_R | PTE_U) != 0)
                  p->killed = 1;
              }
            }
            else{
              p->killed = 1;
            }
          }
        }														// +
        else {
          printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
       ...
      }
      ```

   3. 在 kernel/vm.c 中，修改 uvmunmap() ，让进程销毁时，对于尚未分配实际物理页的虚拟地址，不做处理；修改 uvmcopy()，让进程fork时，对于parent 进程中尚未分配实际物理页的虚拟地址，不做处理。

      ```c
      void
      uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
      {
        ...
        for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
          if((pte = walk(pagetable, a, 0)) == 0)
            // panic("uvmunmap: walk");
            continue;								// +
        ...
      }
          
      int
      uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
      {
        ...  
        for(i = 0; i < sz; i += PGSIZE){
          if((pte = walk(old, i, 0)) == 0)
            // panic("uvmcopy: pte should exist");
            continue;								// +
          if((*pte & PTE_V) == 0)
            // panic("uvmcopy: page not present");
            continue;								// +
       ...
      }    
      ```

   4. 在 kernel/vm.c 中，修改 copyout()，write() 方法将会用到尚未分配物理页的虚拟地址，在 copyout() 中分配物理页；同样修改copyin()，read() 方法将会用到尚未分配物理页的虚拟地址，在 copyin() 中分配物理页。同时要在 vm.c 中添加相应的头文件。 

      ```c
      int
      copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
      {
        uint64 n, va0, pa0;
      
        while(len > 0){
          va0 = PGROUNDDOWN(dstva);
          pa0 = walkaddr(pagetable, va0);
          if(pa0 == 0){								// +
            if(dstva >= myproc()->sz)								
              return -1;
            char *page = kalloc();
            pa0 = (uint64)page;
            memset(page, 0, PGSIZE);
            mappages(myproc()->pagetable, va0, PGSIZE, pa0, PTE_W | PTE_X | PTE_R | PTE_U);
          }											// +
        ...
      }
      
      int
      copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
      {
        uint64 n, va0, pa0;
      
        while(len > 0){
          va0 = PGROUNDDOWN(srcva);
          pa0 = walkaddr(pagetable, va0);
          if(pa0 == 0){								// +
            if(srcva >= myproc()->sz)
              return -1;
            char *page = kalloc();
            pa0 = (uint64)page;
            memset(page, 0, PGSIZE);
            mappages(myproc()->pagetable, va0, PGSIZE, pa0, PTE_W | PTE_X | PTE_R | PTE_U);
          }											// +
        ...
      }
      ```

