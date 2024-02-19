// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);
int cowpage(pagetable_t pagetable, uint64 va);
void* cowalloc(pagetable_t pagetable, uint64 va);
int krefcnt(void* pa);
int kaddrefcnt(void *pa);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct ref_stru{
  struct spinlock lock;
  int cnt[PHYSTOP/PGSIZE];
}ref;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref.lock, "ref");

  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    ref.cnt[(uint64)p/PGSIZE]=1;
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // 只有计数为0的时候才会释放空间
  acquire(&ref.lock);
  if(--ref.cnt[(uint64)pa/PGSIZE]==0){
  // Fill with junk to catch dangling refs.
  release(&ref.lock);
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
  }
  release(&ref.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;
    acquire(&ref.lock);
    ref.cnt[(uint64)r/PGSIZE]=1;
    release(&ref.lock);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

int cowpage(pagetable_t pagetable,uint64 va)
{
  if(va>=MAXVA)
    return -1;
  pte_t* pte = walk(pagetable,va,0);

  if(pte == 0)return -1;
  if((*pte & PTE_V)==0)return -1;

  if(*pte & PTE_F)return 0;
  else return -1;
}


void* cowalloc(pagetable_t pagetable,uint64 va)
{
  if( va % PGSIZE != 0)return 0;

  uint64 pa = walkaddr(pagetable,va);
  if(pa == 0)return 0;

  pte_t* pte = walk(pagetable,va,0);

  if(krefcnt((char*)pa) == 1){
    //如果引用值剩下一个，直接修改对应的pte就可以

    *pte |= PTE_W;
    *pte &= ~PTE_F;//
    return (void*)pa;
  }
  else{
    char *mem = kalloc();//多个进程对这个页面存在引用，那么就要分配新的页面了
    if(mem == 0)return 0;

    memmove(mem,(char *)pa,PGSIZE);
    //因为要重新建立映射，建立映射前对应位置需要无效
    *pte &= ~PTE_V;

    if(mappages(pagetable,va,PGSIZE,(uint64)mem,(PTE_FLAGS(*pte)|PTE_W)&~PTE_F) != 0){
      kfree(mem);
      *pte |= PTE_V;
      return 0;
    }
  //将原来的物理内存引用计数减1
  kfree((char*)PGROUNDDOWN(pa));
  return mem;
  }
}

//获取引用计数
int krefcnt(void* pa)
{
  return ref.cnt[(uint64)pa/PGSIZE];
}

//增加内存的引用计数
int kaddrefcnt(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
  return -1;
  acquire(&ref.lock);
  ++ref.cnt[(uint64)pa / PGSIZE];
  release(&ref.lock);
  return 0;
}
