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

////////////////////////////////////////////////////////////////////
// assignment-1 part-B
///////////////////////////////////////////////////////////////////

uint64 sys_getppid(void){
  struct proc *parent = myproc()->parent;
  if(!parent)
    return -1;
  return parent->pid;
}

uint64 sys_yield(void){
  printf("Yielding the cpu...\n");
  yield();
  printf("got back the control of cpu...\n");
  return 0;
}

uint64 sys_getpa(void){
  int va;
  if(argint(0, &va) < 0)
    return -1;
  return walkaddr(myproc()->pagetable, va) + (va & (PGSIZE-1));
}

uint64 sys_forkf(void){
  int func_addr;
  if(argint(0, &func_addr) < 0)
    return -1;
  return forkf(func_addr);
}

uint64 sys_waitpid(void){
  int pid;
  uint64 addr;
  if(argaddr(1, &addr) < 0)
    return -1;
  if(argint(0, &pid) <0)
    return -1;
  if(pid==-1)
    return wait(addr);
  return waitpid(pid, addr);
}

uint64 sys_ps(void){
  ps();
  return 0;
}

uint64 sys_pinfo(void){
  int pid;
  uint64 addr;
  if(argaddr(1, &addr) < 0)
    return -1;
  if(argint(0, &pid) <0)
    return -1;
  return pinfo(pid, addr);
}