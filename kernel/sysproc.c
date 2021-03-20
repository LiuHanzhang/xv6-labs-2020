#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
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

  backtrace();

  return 0;
}

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

uint64
sys_sigalarm(void)
{
  int n;
  uint64 p;
  if(argint(0, &n) < 0 || argaddr(1, &p) < 0)
    return -1;

  myproc()->alarm_interval = n;
  myproc()->handler = (void (*)())p;
  return 0;
}

uint64
sys_sigreturn(void)
{
  struct proc *p = myproc();

  p->trapframe->epc = p->uregs.epc;
  p->trapframe->ra = p->uregs.ra;
  p->trapframe->sp = p->uregs.sp;
  p->trapframe->gp = p->uregs.gp;
  p->trapframe->tp = p->uregs.tp;
  p->trapframe->t0 = p->uregs.t0;
  p->trapframe->t1 = p->uregs.t1;
  p->trapframe->t2 = p->uregs.t2;
  p->trapframe->s0 = p->uregs.s0;
  p->trapframe->s1 = p->uregs.s1;
  p->trapframe->a0 = p->uregs.a0;
  p->trapframe->a1 = p->uregs.a1;
  p->trapframe->a2 = p->uregs.a2;
  p->trapframe->a3 = p->uregs.a3;
  p->trapframe->a4 = p->uregs.a4;
  p->trapframe->a5 = p->uregs.a5;
  p->trapframe->a6 = p->uregs.a6;
  p->trapframe->a7 = p->uregs.a7;
  p->trapframe->s2 = p->uregs.s2;
  p->trapframe->s3 = p->uregs.s3;
  p->trapframe->s4 = p->uregs.s4;
  p->trapframe->s5 = p->uregs.s5;
  p->trapframe->s6 = p->uregs.s6;
  p->trapframe->s7 = p->uregs.s7;
  p->trapframe->s8 = p->uregs.s8;
  p->trapframe->s9 = p->uregs.s9;
  p->trapframe->s10 = p->uregs.s10;
  p->trapframe->s11 = p->uregs.s11;
  p->trapframe->t3 = p->uregs.t3;
  p->trapframe->t4 = p->uregs.t4;
  p->trapframe->t5 = p->uregs.t5;
  p->trapframe->t6 = p->uregs.t6;

  p->handling = 0;

  return 0;
}