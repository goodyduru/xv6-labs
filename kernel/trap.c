#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

void
ring()
{
  struct proc *current_proc = myproc();
  if ( current_proc->interval == 0 ) {
    return;
  }
  uint xticks;
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  if ( ((xticks - current_proc->last_tick) >= current_proc->interval) && current_proc->handler_returned == 1 )  {
    current_proc->last_tick = xticks;
    //copy from trapframe to alarmframe
    current_proc->alarmframe->epc = current_proc->trapframe->epc;
    current_proc->alarmframe->ra = current_proc->trapframe->ra;
    current_proc->alarmframe->sp = current_proc->trapframe->sp;
    current_proc->alarmframe->t0 = current_proc->trapframe->t0;
    current_proc->alarmframe->t1 = current_proc->trapframe->t1;
    current_proc->alarmframe->t2 = current_proc->trapframe->t2;
    current_proc->alarmframe->s0 = current_proc->trapframe->s0;
    current_proc->alarmframe->s1 = current_proc->trapframe->s1;
    current_proc->alarmframe->a0 = current_proc->trapframe->a0;
    current_proc->alarmframe->a1 = current_proc->trapframe->a1;
    current_proc->alarmframe->a2 = current_proc->trapframe->a2;
    current_proc->alarmframe->a3 = current_proc->trapframe->a3;
    current_proc->alarmframe->a4 = current_proc->trapframe->a4;
    current_proc->alarmframe->a5 = current_proc->trapframe->a5;
    current_proc->alarmframe->a6 = current_proc->trapframe->a6;
    current_proc->alarmframe->a7 = current_proc->trapframe->a7;
    current_proc->alarmframe->s2 = current_proc->trapframe->s2;
    current_proc->alarmframe->s3 = current_proc->trapframe->s3;
    current_proc->alarmframe->s4 = current_proc->trapframe->s4;
    current_proc->alarmframe->s5 = current_proc->trapframe->s5;
    current_proc->alarmframe->s6 = current_proc->trapframe->s6;
    current_proc->alarmframe->s7 = current_proc->trapframe->s7;
    current_proc->alarmframe->s8 = current_proc->trapframe->s8;
    current_proc->alarmframe->s9 = current_proc->trapframe->s9;
    current_proc->alarmframe->s10 = current_proc->trapframe->s10;
    current_proc->alarmframe->s11 = current_proc->trapframe->s11;
    current_proc->alarmframe->t3 = current_proc->trapframe->t3;
    current_proc->alarmframe->t4 = current_proc->trapframe->t4;
    current_proc->alarmframe->t5 = current_proc->trapframe->t5;
    current_proc->alarmframe->t6 = current_proc->trapframe->t6;

    current_proc->handler_returned = 0;

    //set pc to callback
    current_proc->trapframe->epc = current_proc->callback;
  }
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(killed(p))
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
    if ( which_dev == 2 ) {
      ring();
    }
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    setkilled(p);
  }

  if(killed(p))
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to userret in trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

