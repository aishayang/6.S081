#### Implement copy-on write

```markdown
copy-on-write（COW）fork()的目的是：推迟对child的分配和拷贝物理内存页，直到拷贝确实需要。
COW fork()仅仅给 child 创建一个 pagetable，其用户内存的 PTES 指向 parent 的物理页。
COW fork()标记 parent 和 child 的所有用户内存 PTES 是不可写的。
当某个进程尝试写其中一个 COW 页时，cpu 将强制一个 page fault。
kernel page-fault handler检测这种情形，为 faulting 进程分配一页物理内存，复制原始页到新页，
更改 faulting 进程相关 PTE 来指向新页，这次让 PTE 标记为可写。当 page fault handler 返回时，用户进程将能够向拷贝页写入。
COW fork()让物理页（实现用户内存）的释放更有技巧性。
一个给定物理页可能被多个进程的 page table 指向，仅应该在最后的指向消失时，才释放物理页。
```

```c
// 1. kalloc.c 中，新增 int refcount[PHYSTOP/PGSIZE],记录关联物理页的页表数量,新增函数 incref(uint64); 并进行声明.
void
incref(uint64 pa){
  int pn = pa / PGSIZE;
  acquire(&kmem.lock);
  if(pa > PHYSTOP || refcount[pn] < 1)
    panic("incref");
  refcount[pn] += 1;
  release(&kmem.lock);
}

// 2. 修改 kalloc() 与 kfree().
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
	
  // 申请锁是为了防止在同一时刻不同 CPU 核心中的进程对同一页面进行 free;  
  acquire(&kmem.lock);
  int pn = (uint64)pa / PGSIZE;
  if(refcount[pn] < 1)
    panic("kfree ref");
  refcount[pn] -= 1;
  int tmp = refcount[pn];
  release(&kmem.lock);

  if(tmp > 0)
    return;

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;

    // 初始化 refcount[]
    int pn = (uint64)r / PGSIZE; 
    refcount[pn] = 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// 3. 修改kernel/vm.c的uvmcopy()，让进程fork时，不赋值物理页，而是child进程页表指向parent进程的物理页，但标记要将parent和
// child的PTE都清除PTE_W标志位，初始化 refcount[] 计数.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  // char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
      
    pa = PTE2PA(*pte);
    *pte &= ~PTE_W;
    flags = PTE_FLAGS(*pte);
    // if((mem = kalloc()) == 0)
    //   goto err;
    // memmove(mem, (char*)pa, PGSIZE);

    incref(pa);

    if (mappages(new, i, PGSIZE, (uint64)pa, flags) != 0)
    {
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// 6. 将kernel/vm.c中 walk() 定义在defs.h中。-- pte_t * 	walk(pagetable_t, uint64, int);
// 7. 修改kernel/trap.c的usertrap()，并新增 cowfault() 函数, 并在 defs.h 中声明.
int
cowfault(pagetable_t pagetable, uint64 va)
{
  if(va >= MAXVA)
    return -1;

  pte_t *pte = walk(pagetable, va, 0);
  if(pte == 0)
    return - 1;

  if((*pte & PTE_U) == 0 || (*pte & PTE_V) == 0)
    return -1;

  uint64 pa1 = PTE2PA(*pte);

  uint64 pa2 = (uint64)kalloc();
  if(pa2 == 0){
    // panic("cow kalloc failed");
    return -1;
  }

  memmove((void *)pa2, (void *)pa1, PGSIZE);

  kfree((void *)pa1);

  *pte = PA2PTE(pa2) | PTE_V | PTE_U | PTE_R | PTE_W | PTE_X;

  return 0;
}

void
usertrap(void)
{
    ...

  else if((which_dev = devintr()) != 0){
    // ok
  } else if(r_scause() == 15){ // 0xf
      if(cowfault(p->pagetable, r_stval()) < 0)
      	p->killed = 1;
  } else {
	...
}

// 8. 修改kernel/vm.c的copyout(), 因为进行 copy 时使用管道（pipe）会调用 copyout().
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    if(va0 >= MAXVA)
      return -1;

    pte_t *pte = walk(pagetable, va0, 0);
    if(pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0)
      return -1;

    if((*pte & PTE_W) == 0){
      if(cowfault(pagetable, va0) < 0)
        return -1;
    }

    pa0 = PTE2PA(*pte);

    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}
```

总结：通过此次实现加深了对 COW 的理解，对内核中的代码结构以及理解更加清晰。强烈建议观看教授实现这次实验的视频。

