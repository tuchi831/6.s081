#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "elf.h"

static int loadseg(pde_t *pgdir, uint64 addr, struct inode *ip, uint offset, uint sz);

int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint64 argc, sz = 0, sp, ustack[MAXARG+1], stackbase;
  //参数个数，用户栈指针，用户栈的大小
  struct elfhdr elf;
  //生命了一个elfhdr结构体变量elf，用来存储elf文件头信息
  struct inode *ip;
  //生命了一个指向inode结构的指针变量ip，用于表示要执行的可执行文件
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  struct proc *p = myproc();
  
  //开始文件系统的操作
  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);

  // Check ELF header
  // 检查ELF头的有效性
  if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;
  //获取完当前进程后，再获得当前进程的页表
  if((pagetable = proc_pagetable(p)) == 0)
    goto bad;

  // Load program into memory.
  // 循环加载可执行文件的程序段到内存中。遍历可执行文件的所有程序段，对于类型为ELF_PROG_LOAD的程序段,将其加载到内存中的对应虚拟地址处
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    uint64 sz1;
    if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if(sz1 >= PLIC){
      goto bad;
    }
    sz = sz1;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();

  ip = 0;

  p = myproc();
  uint64 oldsz = p->sz;

  // Allocate two pages at the next page boundary.
  // Use the second as the user stack.

  // 为进程分配内存空间并且设置用户栈，sz1表示分配的内存空间的起始地址
  sz = PGROUNDUP(sz);
  uint64 sz1;
  if((sz1 = uvmalloc(pagetable, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  sz = sz1;
  //清除用户栈之上的内存数据，预留一个safe_page
  uvmclear(pagetable, sz-2*PGSIZE);
  sp = sz;
  stackbase = sp - PGSIZE;

  //u2kvmcopy(pagetable, p->kpagetable, 0, sz);

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    // 栈指针向下移动，为参数字符串腾出空间
    sp -= strlen(argv[argc]) + 1;
    //在RISC_v架构中，栈指针sp必须是16字节对齐 的
    sp -= sp % 16; // riscv sp must be 16-byte aligned
    //如果栈指针小于栈的基址，非法
    if(sp < stackbase)
      goto bad;
    //调用copyout函数将当前参数字符串复制到用户栈中
    if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    //将参数字符串在用户栈中的地址存储在ustack数组中，以便后续的代码可以访问这些参数
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // push the array of argv[] pointers.
  // 为用来保存 负责保存参数字符串在用户栈中的地址的ustack
  sp -= (argc+1) * sizeof(uint64);
  sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
    goto bad;
  //
  pte_t *pte,*kernelPte;
  int j;
  //将安全页下的page取消映射
  uvmunmap(p->kpagetable,0,PGROUNDUP(oldsz)/PGSIZE,0);
  
  for(j = 0;j < sz;j+=PGSIZE){
    pte = walk(pagetable,j,0);
    kernelPte = walk(p->kpagetable,j,1);
    *kernelPte = (*pte) & ~PTE_U;
  }


 //







  //
  // arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  p->trapframe->a1 = sp;
  // 将用户栈的地址sp存储在进程的陷阱帧中的寄存器a1中。这样做事为了将用户栈的地址传递给用户程序的main函数作为参数

  // Save program name for debugging.
  // 进程的初始状态设置

  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  
  // 使用safestrcpy函数将程序的名称复制到进程的name字段中
  safestrcpy(p->name, last, sizeof(p->name));
    
  //上面的代码初始化了新页表，我们现在是要保存旧页表，然后传入新的页表
  //保存旧的页表来释放新的页表
  
  // Commit to the user image.
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  p->trapframe->epc = elf.entry;  // initial program counter = main
  p->trapframe->sp = sp; // initial stack pointer
  proc_freepagetable(oldpagetable, oldsz);
  if(p->pid==1) vmprint(p->pagetable);
  return argc; // this ends up in a0, the first argument to main(argc, argv)

 bad:
  if(pagetable)
    proc_freepagetable(pagetable, sz);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}

// Load a program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int
loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz)
{
  uint i, n;
  uint64 pa;

  if((va % PGSIZE) != 0)
    panic("loadseg: va must be page aligned");

  for(i = 0; i < sz; i += PGSIZE){
    pa = walkaddr(pagetable, va + i);
    if(pa == 0)
      panic("loadseg: address should exist");
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, 0, (uint64)pa, offset+i, n) != n)
      return -1;
  }
  
  return 0;
}
