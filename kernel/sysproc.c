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
  backtrace();
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

uint64
sys_sigalarm(void)
{
  int interval;
  uint64 handler_addr;
  uint xticks;
  argint(0, &interval);
  argaddr(1, &handler_addr);
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  struct proc *current_proc = myproc();
  current_proc->interval = interval;
  current_proc->callback = handler_addr;
  current_proc->last_tick = xticks;
  current_proc->handler_returned = 1;
  return 0;
}

uint64
sys_sigreturn(void)
{
  struct proc *current_proc = myproc();
  current_proc->trapframe->epc = current_proc->alarmframe->epc;
  current_proc->trapframe->ra = current_proc->alarmframe->ra;
  current_proc->trapframe->sp = current_proc->alarmframe->sp;
  current_proc->trapframe->t0 = current_proc->alarmframe->t0;
  current_proc->trapframe->t1 = current_proc->alarmframe->t1;
  current_proc->trapframe->t2 = current_proc->alarmframe->t2;
  current_proc->trapframe->s0 = current_proc->alarmframe->s0;
  current_proc->trapframe->s1 = current_proc->alarmframe->s1;
  current_proc->trapframe->a1 = current_proc->alarmframe->a1;
  current_proc->trapframe->a2 = current_proc->alarmframe->a2;
  current_proc->trapframe->a3 = current_proc->alarmframe->a3;
  current_proc->trapframe->a4 = current_proc->alarmframe->a4;
  current_proc->trapframe->a5 = current_proc->alarmframe->a5;
  current_proc->trapframe->a6 = current_proc->alarmframe->a6;
  current_proc->trapframe->a7 = current_proc->alarmframe->a7;
  current_proc->trapframe->s2 = current_proc->alarmframe->s2;
  current_proc->trapframe->s3 = current_proc->alarmframe->s3;
  current_proc->trapframe->s4 = current_proc->alarmframe->s4;
  current_proc->trapframe->s5 = current_proc->alarmframe->s5;
  current_proc->trapframe->s6 = current_proc->alarmframe->s6;
  current_proc->trapframe->s7 = current_proc->alarmframe->s7;
  current_proc->trapframe->s8 = current_proc->alarmframe->s8;
  current_proc->trapframe->s9 = current_proc->alarmframe->s9;
  current_proc->trapframe->s10 = current_proc->alarmframe->s10;
  current_proc->trapframe->s11 = current_proc->alarmframe->s11;
  current_proc->trapframe->t3 = current_proc->alarmframe->t3;
  current_proc->trapframe->t4 = current_proc->alarmframe->t4;
  current_proc->trapframe->t5 = current_proc->alarmframe->t5;
  current_proc->trapframe->t6 = current_proc->alarmframe->t6;
  current_proc->handler_returned = 1;
  return current_proc->alarmframe->a0;
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
