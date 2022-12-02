#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
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
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
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


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
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
  // lab pgtbl: your code here.
  uint64 start_addr, res_addr, va;
  int num_pages;
  unsigned int temp;
  pte_t *pte;
  pagetable_t userpg = myproc()->pagetable;
  int max_pages = 1L << 27;
  argaddr(0, &start_addr);
  argint(1, &num_pages);
  argaddr(2, &res_addr);
  if ( num_pages > max_pages )
    num_pages = max_pages;
  
  temp = 0;
  start_addr = PGROUNDDOWN(start_addr);
  for ( int i = 0; i < num_pages; i++ ) {
    va = start_addr+i*PGSIZE;
    pte = walk(userpg, va, 0);
    if ( pte == 0 ) 
      continue;
    
    if ( (*pte & PTE_V) == 0 )
      continue;

    if ( (*pte & PTE_A) != 0 ) {
      temp |= (1 << i); //set bitmask
      *pte &= ~PTE_A; //clear accessed in pte
    }
  }
  copyout(userpg, res_addr, (char *)&temp, sizeof(temp));
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
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

// return trace information for system call
uint64
sys_trace(void)
{
  int mask;
  argint(0, &mask);
  myproc()->syscall_mask = mask;
  return 0;
}

// return system information
uint64
sys_sysinfo(void)
{
  uint64 addr;
  argaddr(0, &addr);
  struct sysinfo info;
  info.freemem = kfreememsize();
  info.nproc = num_of_used_proc();
  if ( copyout(myproc()->pagetable, addr, (char *)&info, sizeof(info)) < 0 )
    return -1;
  return 0;
}