预备知识：

- xv6 book 第三章，
- 内核布局代码 kernel/memlayout.h，
- 虚拟内存相关带码 kernel/vm.c，
- 分配、释放物理内存代码 kernel/kalloc.c

1. #### print a page table

   任务需求：

   ```markdown
   为了帮助学习RISC-V page tables，也可能是帮助未来的调试，第一个任务是写一个函数来打印页表内容。
   定义一个函数vmprint()。它应该接收一个pagetable_t参数，并且用下面格式打印页表。
   在exec.c中return argc之前，插入if(p->pid==1) vmprint(p-pagetable)，来打印第一个进程的页表。
   当启动xv6时，应该打印以下内容，描述第一个进程的页表信息，在刚执行完exec()时。
   ```

   ```c
   // 在 kernel/defs.h 添加中声明,在 kernel/vm.c 中添加函数实现
   // vmprintf()的实现
   void travel_pt(pagetable_t pagetable, int level)
   {
     for (int i = 0; i < 512; i++){
       pte_t pte = pagetable[i];
       if(pte & PTE_V){
         uint64 pa = PTE2PA(pte);
         if(level == 0){                                 // level 0
           printf("..%d: pte %p pa %p\n", i, pte, pa);
           travel_pt((pagetable_t)pa, level + 1);
         }
         else if(level == 1){                            // level 1
           printf(".. ..%d: pte %p pa %p\n", i, pte, pa);
           travel_pt((pagetable_t)pa, level + 1);
         }
         else{                                           // level 2
           printf(".. .. ..%d: pte %p pa %p\n", i, pte, pa);
         }
       }
     }
   }
   
   void vmprint(pagetable_t pagetable)
   {
     printf("page table %p\n", pagetable);
   
     travel_pt(pagetable, 0);
   }
   ```

   

2. #### A kernel page table per process

   任务需求：

   ```markdown
   无论何时在内核执行时，xv6使用同一个内核页表。内核页表是一个物理地址的直接映射，因此内核虚拟地址x对应物理地址x。
   xv6也有一个单独的页表给每个进程的用户地址空间，仅包含那个进程用户内存的映射，起始于虚拟地址0。
   因为内核页表不包含这些映射，用户地址在内核无效。因此，当内核需要使用一个用户指针传到system call时，内核必须首先翻译指针到物理地址。
   这个和下个实验的目的是为了允许内核直接解析用户指针。
   第一个任务是更改内核，为了当在内核执行时，每个进程使用它自己的内核页表拷贝。
   更改struct proc来让每个进程保持一个内核页表，更改scheduler()，当切换进程时切换内核页表。
   对于这一步，每个进程的内核页表应该和已存在的全局内核页表完全相同。
   读xv6书、理解作业一开始提到的代码如何起作用，将更容易正确地更改虚拟内存代码。
   页表设置中的bug会因为缺少映射导致缺陷，导致加载、存储会影响到不可预期的物理内存页，也会导致执行不正确的物理内存页。
   ```

   跟着提示走：

     1. 在 proc 结构体中添加 kpagetable 字段

        ```c
        struct proc {
          struct spinlock lock;
          ...
          pagetable_t pagetable;       // User page table
          pagetable_t kpagetable;      // Kernel page table
          struct trapframe *trapframe; // data page for trampoline.S
          ...
        };
        ```

   2. 在 vm.c 中模仿 kvminit() 实现一个内核映射函数（kvmmake()）的构建，后面每一个进程都调用这个函数构建一份内核表

      ```c
      pagetable_t
      kvmmake()
      {
        pagetable_t kpagetable = (pagetable_t) kalloc();
        memset(kpagetable, 0, PGSIZE);
      
        // 使用 kvmmap 函数需要在kvmmap中添加一个 pagetable_t 参数, 也可以使用不添加 pagetable_t参数的mappages函数
        // kvmmap(pagetable_t kpagetable, uint64 va, uint64 pa, uint64 sz, int perm)
        // mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
          
        // uart registers
        kvmmap(kpagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W);
      
        // virtio mmio disk interface
        kvmmap(kpagetable, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
      
        // CLINT
        kvmmap(kpagetable, CLINT,  CLINT, 0x10000, PTE_R | PTE_W);
      
        // PLIC
        kvmmap(kpagetable, PLIC, PLIC, 0x400000, PTE_R | PTE_W);
      
        // map kernel text executable and read-only.
        kvmmap(kpagetable, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);
      
        // map kernel data and the physical RAM we'll make use of.
        kvmmap(kpagetable, (uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);
      
        // map the trampoline for trap entry/exit to
        // the highest virtual address in the kernel.
        kvmmap(kpagetable, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
      
        return kpagetable;
      }
      
      /*
       * create a direct-map page table for the kernel.
       */
      void
      kvminit()
      {
        kernel_pagetable = kvmmake();
      }
      
      ...
      void
      kvmmap(pagetable_t kpagetable, uint64 va, uint64 pa, uint64 sz, int perm)
      {
          ...
      }
      ```

   3. 在 proc.c 的 procinit() 函数中删除 kernel stack 的映射，在 allocproc() 函数中新增 kernel stack 映射

      ```c
      void
      procinit(void)
      {
        struct proc *p;
        
        initlock(&pid_lock, "nextpid");
        for(p = proc; p < &proc[NPROC]; p++) 
            initlock(&p->lock, "proc");
      
            // Allocate a page for the process's kernel stack.
            // Map it high in memory, followed by an invalid
            // guard page.
          
          // 删除 kernel stack 映射
            // char *pa = kalloc();
            // if(pa == 0)
            //   panic("kalloc");
            // uint64 va = KSTACK((int) (p - proc));
            // kvmmap(va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
            // p->kstack = va;
        // kvminithart();
      }
      
      static struct proc*
      allocproc(void)
      {
        struct proc *p;
        ...
      found:
         p->pid = allocpid();
        // Allocate a trapframe page.
        ...
            
        // (+) A kernel page table
        if((p->kpagetable = kvmmake()) == 0){
          freeproc(p);
          release(&p->lock);
          return 0;
        }
      
        // (+) Allocate a page for the process's kernel stack.
        // Map it high in memory, followed by an invalid
        // guard page.
        char *pa = kalloc();
        if(pa == 0)
          panic("kalloc");
        uint64 va = KSTACK((int)(p - proc));
        mappages(p->kpagetable, va, PGSIZE, (uint64)pa, PTE_R | PTE_W);
        p->kstack = va;
        ...
      }
      ```

   4. 在 proc.c 中的scheduler() 函数中，有新进程运行就使用新进程的内核页表，没有运行进程时就用kernel_pagetable

      ```c
      void 
      scheduler(void)
      {
          ...
       p->state = RUNNING;
              c->proc = p;
      
              w_satp(MAKE_SATP(p->kpagetable)); // +
              sfence_vma();                     // +
      
              swtch(&c->context, &p->context);
      
              // Process is done running for now.
              // It should have changed its p->state before coming back.
              c->proc = 0;
      
               // Use kernel_pagetable when no process is running.
              kvminithart();                   // +
      		...
      }
      ```

   5. 在 proc.c 中的 freeproc() 里面释放进程内核页表，注意不要释放物理内存：

      ```c
      static void
      freeproc(struct proc *p)
      {
        if(p->trapframe)
        	kfree((void*)p->trapframe);
        p->trapframe = 0;
      
        // free kernel page table
        if(p->kstack)                                // +
          uvmunmap(p->kpagetable, p->kstack, 1, 1);  // +
        p->kstack = 0;                               // +
        if (p->kpagetable)						   // +
          kvmfree(p->kpagetable);					   // +
        p->kpagetable = 0;						   // +
          
        ...
      }
      
      // 其中 kvmfree() 模仿 freewalk()，释放内核页表
      void
      kvmfree(pagetable_t kpagetable)
      {
        for (int i = 0; i < 512; i++){
          pte_t pte = kpagetable[i];
          if(pte & PTE_V){
            kpagetable[i] = 0;
            if((pte & (PTE_R | PTE_W | PTE_X)) == 0){
              uint64 child = PTE2PA(pte);
              kvmfree((pagetable_t)child);
            }
          }
          else if(pte & PTE_V){
            panic("kvmfree: leaf");
          }
        }
        kfree((void *)kpagetable);
      }
      ```

   6. 在 vm.c 中修改 kvmpa() 中的 walk

      ```c
      uint64
      kvmpa(uint64 va)
      {
        ...
            
        pte = walk(myproc()->kpagetable, va, 0);
        
        ...
      }
      ```




3. #### Simplify  `copyin/copyinstr`

   任务需求：

   ```markdown
   内核的copyin函数读取用户指针指向的内存。它先将它们翻译为物理地址（内核可以直接用）。通过代码walk进程页表实现翻译。
   在此实验中，你的工作是给每个进程的内核页表添加用户映射，使得copyin可以直接使用用户指针。
   ```

   1. 在 kernel/defs.h 中声明 copyin_new() 和 copyinstr_new()， 然后修改 vm.c 中 copyin() 和 copyinstr()
   
      ```c
      int
      copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
      {
        return copyin_new(pagetable, dst, srcva, len);
      }
      int
      copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
      {
        return copyinstr_new(pagetable, dst, srcva, max);
      }
      ```
   
   2. 新增函数 uvm2k() 用于复制用户页表到内核中，在 defs.h 中声明，注意复制的用户页表虚拟地址不能超过 PLIC，因为在 PLIC 上面时 kernel 的地址空间
   
      ```c
      // Copy mapping form U to K
      int
      uvm2k(pagetable_t pagetable, pagetable_t kpagetable, uint64 oldsz, uint64 newsz){
        pte_t *pte, *k_pte;
      
        if(oldsz > newsz || newsz >= PLIC)
          return -1;
        for (uint64 va = oldsz; va < newsz; va += PGSIZE)
        {
          if ((pte = walk(pagetable, va, 0)) == 0)
      		  panic("copyin_new: pte not exist");
      	  if ((k_pte = walk(kpagetable, va, 1)) == 0)
      		  panic("copyin_new: walk failed");
      
          *k_pte = (*pte) & ~PTE_U;
        }
        return 0;
      }
      ```
   
   3. 在 fork(), exec(), sbrk()->growproc(), userinit() 中添加 uvm2k()
   
      ```c
      void
      userinit(void)
      {
        ...
        p->state = RUNNABLE;
      
        uvm2k(p->pagetable, p->kpagetable, 0, p->sz); // +
      
        release(&p->lock);
      }
      
      int
      growproc(int n)
      {
        ...
        sz = p->sz;
        if(n > 0){
          if(PGROUNDUP(sz+n) >= PLIC)  // +
            return -1;				 // +
          if ((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0)
          {
            return -1;
          }
        }
        else if (n < 0)
        {
          sz = uvmdealloc(p->pagetable, sz, sz + n);
        }
        uvm2k(p->pagetable, p->kpagetable, p->sz, sz); // +
        p->sz = sz;
        return 0;
      }
      
      int
      fork(void)
      {
        ...
        np->state = RUNNABLE;
      
        int ret = uvm2k(np->pagetable, np->kpagetable, 0, np->sz);	// +
        if(ret < 0){													// +
          freeproc(np);												// +
          release(&np->lock);											// +
          return -1;													// +
        }																// +
        ...
      }
      
      int
      exec(char *path, char **argv)
      { 
        ...
        if(p->pid == 1)
          vmprint(p->pagetable);
          
        uvm2k(p->pagetable, p->kpagetable, 0, p->sz);  							// +
      
        return argc; // this ends up in a0, the first argument to main(argc, argv)
        ...
      }
      ```
   
      

所有实验测试结果：

​			![img](file:///C:\Users\aisha08\AppData\Roaming\Tencent\Users\1429402802\QQ\WinTemp\RichOle\1F774AS6Q]2LN9ZLF8VVA3P.png)

总结：这次实验加深了对三级页表的理解，深入理解了用户页表向内核页表映射的过程，以及映射的限制，用户态具有页表为什么还需要向内核态映射。

