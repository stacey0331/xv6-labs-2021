#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  uint64 start_addr;
  if(argaddr(0, &start_addr) < 0)
    return -1;

  int num_pages;
  if(argint(1, &num_pages) < 0)
    return -1;
  
  uint64 user_addr;
  if(argaddr(2, &user_addr) < 0)
    return -1;

  // // upper limit on number of pages
  // if (num_pages > 1024) return -1; 

  uint64 bitmask = 0;
  for(int i = 0; i < num_pages; i++) {
    pte_t * curr_pte = walk(myproc()->pagetable, start_addr + i*PGSIZE, 0);
    if ((*curr_pte & PTE_V) && (*curr_pte & PTE_A)) {
      bitmask |= (1 << i);
      *curr_pte &= (~PTE_A);
    }
  }
  
  return copyout(myproc()->pagetable, user_addr, (void *)&bitmask, num_pages);
}
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
