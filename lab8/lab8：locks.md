1. Memory allocator

   1. 你的工作是实现每个CPU freelist，当一个CPU 的freelist使用完时可以stealing 其他CPU中的非使用的freelist。

   2. 实现：

      1. ```c
         // 1. 修改kernel/kalloc.c，修改原有结构体声明kmem，定义结构体数组kmems[NCPU]
         struct kmem{
           struct spinlock lock;
           struct run *freelist;
         } kmems[NCPU];
         
         // 2. 修改kernel/kalloc.c，修改kinit方法，对所有的锁进行初始化
         void
         kinit()
         {
           for (int i = 0; i < NCPU; i++){
             initlock(&kmems[i].lock, "kmem");
           }
           freerange(end, (void*)PHYSTOP);
         }
         
         // 3. 修改kernel/kalloc.c，修改kfree方法，获取当前cpuid，释放对应freelist。
         void
         kfree(void *pa)
         {
           struct run *r;
         
           if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
             panic("kfree");
         
           // Fill with junk to catch dangling refs.
           memset(pa, 1, PGSIZE);
         
           r = (struct run*)pa;
         
           push_off();	// 关闭中断
           int cpu_id = cpuid();
           acquire(&kmems[cpu_id].lock);
           r->next = kmems[cpu_id].freelist;
           kmems[cpu_id].freelist = r;
           release(&kmems[cpu_id].lock);
           pop_off();	// 打开中断
         }
         
         // 4. 修改kernel/kalloc.c，修改kalloc方法，当前CPU freelist为空时，从其他CPU freelist处获取。
         void *
         kalloc(void)
         {
           struct run *r;
         	
           push_off();	// 关闭中断
           int cpu_id = cpuid();
           acquire(&kmems[cpu_id].lock);
           r = kmems[cpu_id].freelist;
           if(r)
             kmems[cpu_id].freelist = r->next;
           else {
             for (int i = (cpu_id + 1) % NCPU, j = 0; j < NCPU - 1; i = (i + 1) % NCPU, j++){
               if(kmems[i].freelist){
                 acquire(&kmems[i].lock);
                 r = kmems[i].freelist;
                 kmems[i].freelist = r->next;
                 release(&kmems[i].lock);
                 break;
               }
             }
           }
           release(&kmems[cpu_id].lock);
           pop_off();	// 打开中断
         
           if(r)
             memset((char*)r, 5, PGSIZE); // fill with junk
           return (void*)r;
         }
         ```

2. Buffer cache

   1. 修改块缓存，使得在运行 bcachetest 时，bcache 中所有锁的获取循环迭代次数接近于零。理想情况下，块缓存中涉及的所有锁的计数总和应该为零，但如果总和小于 500 就可以了。

   2. 实现：

      1. ```c
         // 1. 修改kernel/buf.h，新增tick，作为时间标记。
         struct buf {
           ...
           uint tick;	//+
         };
         
         // 2. 修改kernel/bio.c，新增NBUCKET，作为hash表bucket数量；修改bcache，去除head；新增哈希表结构题数组。
         #define NBUCKET 13
         struct {
           struct spinlock lock;
           struct buf buf[NBUF];
         } bcache;
         struct bMem {
           struct spinlock lock;
           struct buf head;
         };
         static struct bMem hashTable[NBUCKET];
         
         // 3. 修改kernel/bio.c，在binit方法中新增哈希表中每个bucket lock初始化。
         void
         binit(void)
         {
           struct buf *b;
         
           initlock(&bcache.lock, "bcache");
         
           for (int i = 0; i < NBUCKET; i++){
             initlock(&hashTable[i].lock, "hashbacket");
           }
           for (b = bcache.buf; b < bcache.buf + NBUF; b++)
           {
             initsleeplock(&b->lock, "buffer");
           }
         }
         
         // 4. 修改kernel/bio.c，新增replaceBuffer方法，用于LRU buffer赋值。
         void 
         replaceBuffer(struct buf *lruBuf, uint dev, uint blockno, uint ticks){
           lruBuf->dev = dev;
           lruBuf->blockno = blockno;
           lruBuf->valid = 0;
           lruBuf->refcnt = 1;
           lruBuf->tick = ticks;
         }
         
         // 5. 修改kernel/bio.c中的bget方法。
         static struct buf*
         bget(uint dev, uint blockno)
         {
            struct buf *b;
         	struct buf *lastBuf;
         
         	// Is the block already cached?
           uint64 num = blockno % NBUCKET;
           acquire(&(hashTable[num].lock));
           for(b = hashTable[num].head.next, lastBuf = &(hashTable[num].head); b; b = b->next){
         		if (!(b->next)){
         			lastBuf = b;
         		}
         		if(b->dev == dev && b->blockno == blockno){
         			b->refcnt++;
         			release(&(hashTable[num].lock));
         			acquiresleep(&b->lock);
         			return b;
         		}
         	}
         
         	struct buf *lruBuf = 0;
         	acquire(&bcache.lock);
         	for(b = bcache.buf; b < bcache.buf + NBUF; b++){
         	    if(b->refcnt == 0) {
         	    	if (lruBuf == 0){
         	    		lruBuf = b;
         	    		continue;
         	    	}
         			if (b->tick < lruBuf->tick){
         				lruBuf = b;
         			}
         	    }
           	}
         
         	if (lruBuf){
         	  	uint64 oldTick = lruBuf->tick;
               uint64 oldNum = (lruBuf->blockno) % NBUCKET;
               if (oldTick == 0)
               {
                 replaceBuffer(lruBuf, dev, blockno, ticks);
                 lastBuf->next = lruBuf;
                 lruBuf->prev = lastBuf;
               }
               else
               {
                 if (oldNum != num)
                 {
                   acquire(&(hashTable[oldNum].lock));
                   replaceBuffer(lruBuf, dev, blockno, ticks);
                   lruBuf->prev->next = lruBuf->next;
                   if (lruBuf->next)
                   {
                     lruBuf->next->prev = lruBuf->prev;
                   }
                   release(&(hashTable[oldNum].lock));
                   lastBuf->next = lruBuf;
                   lruBuf->prev = lastBuf;
                   lruBuf->next = 0;
                 }
                 else
                 {
                   replaceBuffer(lruBuf, dev, blockno, ticks);
                 }
               }
               release(&bcache.lock);
         		release(&(hashTable[num].lock));
         		acquiresleep(&lruBuf->lock);
         		return lruBuf;
         	}
           panic("bget: no buffers");
         }
         
         // 6. 修改kernel/bio.c中的brelse、bpin、bunpin方法。
         void
         brelse(struct buf *b)
         {
           if(!holdingsleep(&b->lock))
             panic("brelse");
         
           releasesleep(&b->lock);
         
           uint64 num = (b->blockno) % NBUCKET;
           acquire(&hashTable[num].lock);
           b->refcnt--;
           release(&hashTable[num].lock);
         }
         
         void
         bpin(struct buf *b) {
           uint64 num = (b->blockno) % NBUCKET;
           acquire(&hashTable[num].lock);
           b->refcnt++;
           release(&hashTable[num].lock);
         }
         
         void
         bunpin(struct buf *b) {
           uint64 num = (b->blockno) % NBUCKET;
           acquire(&hashTable[num].lock);
           b->refcnt--;
           release(&hashTable[num].lock);
         }
         ```

         