#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "procstat.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// maintain the start time of cpu burst;
int start_time;

// variables to print statistics;
int curr_no_proc;               // number of process of the batch currently in execution.
int total_no_proc;              // number of process in the batch;

int batch_start_time;           // batch start time;

int total_trnarnd_time;         // sum of turnaround time of all process of the batch.
int total_wait_time;            // sum of waiting time of all process of the batch.

// complition time related 
int total_cmpl_time;
int max_cmpl_time;              // maximum time to complete a process of the batch.
int min_cmpl_time;              // minimum time to complete a process of the batch.

// cpu bursts related
int num_cpu_burst;              // number of cpu burst of the batch.
int total_cpu_burst;            // sum of length of cpu bursts.
int max_cpu_burst;
int min_cpu_burst;

// estimated cpu bursts related
int num_estm_burst;
int tot_estm_burst;
int max_estm_burst;
int min_estm_burst;

// cpu burst estimation error related
int total_abs_error; 
int num_error; 


int min(int a, int b){
  if(a<b)
  return a;
  return b;
}
int max(int a, int b){
  if(a<b)
  return b;
  return a;
}


int abs(int x){
  if(x<0)
  return -x;
  return x;
}

void estimate_cpu_burst(struct proc *p){
  int lcpub;
  
  if(!holding(&tickslock)){
    acquire(&tickslock);
    lcpub = ticks - start_time;
    release(&tickslock);
  }
  else lcpub = ticks - start_time;
  
  int estimate = p->last_estimate;
  
  if(lcpub!=0){
    total_cpu_burst += lcpub;
    num_cpu_burst ++ ;
    max_cpu_burst = max(max_cpu_burst, lcpub);
    min_cpu_burst = min(min_cpu_burst, lcpub);
  }
  if(estimate!=0){
    tot_estm_burst += estimate;
    num_estm_burst ++;
    max_estm_burst = max(max_estm_burst, estimate);
    min_estm_burst = min(min_estm_burst, estimate);
  }
  if(lcpub!=0 && estimate!=0){
    num_error++;
    total_abs_error += abs(lcpub - estimate);
  }
  p->last_estimate = lcpub - (SCHED_PARAM_SJF_A_NUMER*lcpub)/SCHED_PARAM_SJF_A_DENOM + (SCHED_PARAM_SJF_A_NUMER*p->last_estimate)/SCHED_PARAM_SJF_A_DENOM;
}

// get current time.

int get_curr_time(void){
  if(!holding(&tickslock)){
    acquire(&tickslock);
    int xticks = ticks;
    release(&tickslock);
    return xticks;
  }
  else return ticks;
}



// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl) {
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table at boot time.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->kstack = KSTACK((int) (p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void) {
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void) {
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid() {
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  uint xticks;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);

  p->ctime = xticks;
  p->stime = -1;
  p->endtime = -1;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// Create a user page table for a given process,
// with no user memory, but with trampoline pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe just below TRAMPOLINE, for trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// od -t xC initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy init's instructions
  // and data into it.
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  np->basepriority = 0;
  np->priority = 0;
  np->batch = 0;
  
  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

int
forkf(uint64 faddr)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;
  // Make child to jump to function
  np->trapframe->epc = faddr;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  np->basepriority = 0;
  np->priority = 0;
  np->batch = 0;

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();
  uint xticks;

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);

  p->endtime = xticks;

  if(p->batch){
    estimate_cpu_burst(p);
    curr_no_proc -= 1;
    total_trnarnd_time += xticks - p->ctime;

    total_cmpl_time += xticks;
    max_cmpl_time = max(max_cmpl_time, xticks);
    min_cmpl_time = min(min_cmpl_time, xticks);
    
    if(curr_no_proc == 0){
      // the last batch process.
      // print all the stats

      int btch_exc_time = xticks - batch_start_time;
      int btch_avg_trnarnd_time = total_trnarnd_time / total_no_proc;

      printf("\nBatch execution time: %d\n", btch_exc_time);
      printf("Average turn-around time: %d\n", btch_avg_trnarnd_time);
      printf("Average Waiting time: %d\n", total_wait_time / total_no_proc);
      printf("Completion time: avg: %d, max: %d, min: %d\n", total_cmpl_time / total_no_proc, max_cmpl_time, min_cmpl_time);
      if(schedpol == SCHED_NPREEMPT_SJF){
        printf("CPU bursts: count: %d, avg: %d, max: %d, min: %d\n",num_cpu_burst, total_cpu_burst / num_cpu_burst, max_cpu_burst, min_cpu_burst );
        printf("CPU bursts estimates: count: %d, avg: %d, max: %d, min: %d\n",num_estm_burst, tot_estm_burst / num_estm_burst, max_estm_burst, min_estm_burst );
        printf("CPU burst estimation error: count: %d, avg: %d", num_error, total_abs_error / num_error);
      }
    }
  }
  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      if(np->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&np->lock);

        havekids = 1;
        if(np->state == ZOMBIE){
          // Found one.
          pid = np->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                  sizeof(np->xstate)) < 0) {
            release(&np->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(np);
          release(&np->lock);
          release(&wait_lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || p->killed){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

int
waitpid(int pid, uint64 addr)
{
  struct proc *np;
  struct proc *p = myproc();
  int found=0;

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for child with pid
    for(np = proc; np < &proc[NPROC]; np++){
      if((np->parent == p) && (np->pid == pid)){
	found = 1;
        // make sure the child isn't still in exit() or swtch().
        acquire(&np->lock);

        if(np->state == ZOMBIE){
           if(addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                  sizeof(np->xstate)) < 0) {
             release(&np->lock);
             release(&wait_lock);
             return -1;
           }
           freeproc(np);
           release(&np->lock);
           release(&wait_lock);
           return pid;
	}

        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!found || p->killed){
      release(&wait_lock);
      return -1;
    }

    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  int last_sched_pol;
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();
    last_sched_pol = schedpol;

    
    /////////////////////////////////
    // UNIX style scheduler
    /////////////////////////////////

    
    if(schedpol == SCHED_PREEMPT_UNIX){
      
      int min_priority = __INT_MAX__;
      struct proc *min_priority_proc = 0;
      int flag = 1;

      for(p=proc;p<&proc[NPROC];p++){
      
        // if(last_sched_pol != schedpol){
        //   break;
        // }


        acquire(&p->lock);
      
        if(p->state == RUNNABLE && !p->batch){
          p->state = RUNNING;
          c->proc = p;

          swtch(&c->context, &p->context);

          // Process is done running for now.
          // It should have changed its p->state before coming back.
          c->proc = 0;
          flag = 0;
          release(&p->lock);
          break;
        }
      
        else if(p->state == RUNNABLE){
          // update priority
          p->cpu_usage /= 2;
          p->priority = p->basepriority + p->cpu_usage/2;

          if(min_priority > p->priority){
            min_priority = p->priority;
            min_priority_proc = p;
          }
        }
        release(&p->lock);
      }

      if(flag && min_priority_proc){
        
        acquire(&min_priority_proc->lock);
        
        min_priority_proc->state = RUNNING;
        c->proc = min_priority_proc;
        
        // update start time to compute this cpu burst.
        start_time = get_curr_time();
        total_wait_time += start_time - min_priority_proc->wait_strt_time;
        // context switch
        // printf("scheduled process pid: %d, priority value: %d\n", min_priority_proc->pid, min_priority_proc->priority);
        swtch(&c->context, &min_priority_proc->context);
        c->proc = 0;

        release(&min_priority_proc->lock);
      }
    }

    
    /////////////////////////////////
    // SJF
    /////////////////////////////////


    if(schedpol == SCHED_NPREEMPT_SJF){

      int min_cpu_burst = __INT_MAX__;
      struct proc *minp = 0;
      int flag=1;
      
      for(p=proc;p<&proc[NPROC];p++){
      
        if(last_sched_pol != schedpol){
          break;
        }
      
        acquire(&p->lock);
      
        if(p->state == RUNNABLE && !p->batch){
          p->state = RUNNING;
          c->proc = p;

          swtch(&c->context, &p->context);

          // Process is done running for now.
          // It should have changed its p->state before coming back.
          c->proc = 0;
          flag = 0;
          release(&p->lock);
          break;
        }
      
        else if(p->state == RUNNABLE){
          if(min_cpu_burst > p->last_estimate){
            min_cpu_burst = p->last_estimate;
            minp = p;
          }
        }
        release(&p->lock);
      }
      if(flag && minp){
        
        acquire(&minp->lock);
        
        minp->state = RUNNING;
        c->proc = minp;
        
        // update start time to compute this cpu burst.
        start_time = get_curr_time();
        total_wait_time += start_time - minp->wait_strt_time;
        
        // context switch
        swtch(&c->context, &minp->context);
        c->proc = 0;

        release(&minp->lock);
      }
    }


    /////////////////////////////////
    // FCFS
    /////////////////////////////////

    if(schedpol == SCHED_NPREEMPT_FCFS){
      int min_ctime = __INT_MAX__;
      struct proc *minp = 0;
      int flag=1;
      
      for(p=proc;p<&proc[NPROC];p++){
      
        if(last_sched_pol != schedpol){
          break;
        }
      
        acquire(&p->lock);
      
        if(p->state == RUNNABLE && !p->batch){
          p->state = RUNNING;
          c->proc = p;

          swtch(&c->context, &p->context);

          // Process is done running for now.
          // It should have changed its p->state before coming back.
          c->proc = 0;
          flag = 0;
          release(&p->lock);
          break;
        }
      
        else if(p->state == RUNNABLE){
          if(min_ctime > p->ctime){
            min_ctime = p->ctime;
            minp = p;
          }
        }
        release(&p->lock);
      }
      if(flag && minp){
        
        acquire(&minp->lock);
        
        minp->state = RUNNING;
        c->proc = minp;
        
        // update start time to compute this cpu burst.
        start_time = get_curr_time();
        total_wait_time += start_time - minp->wait_strt_time;
        
        // context switch
        swtch(&c->context, &minp->context);
        c->proc = 0;

        release(&minp->lock);
      } 
    }


    /////////////////////////////////
    // Round Robin
    /////////////////////////////////


    if(schedpol == SCHED_PREEMPT_RR){
      for(p = proc; p < &proc[NPROC]; p++) {
        if(last_sched_pol != schedpol){
          break;
        }
        acquire(&p->lock);
        if(p->state == RUNNABLE) {
          // Switch to chosen process.  It is the process's job
          // to release its lock and then reacquire it
          // before jumping back to us.
          p->state = RUNNING;
          c->proc = p;
          
          if(p->batch){
            start_time = get_curr_time();
            total_wait_time += start_time - p->wait_strt_time;
          }

          swtch(&c->context, &p->context);

          // Process is done running for now.
          // It should have changed its p->state before coming back.
          c->proc = 0;
        }
        release(&p->lock);
      }
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.

void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  
  if(p->batch){
    estimate_cpu_burst(p);
    // printf("in yield, adding 200 to cpu, PID: %d\n", p->pid);
    p->cpu_usage += SCHED_PARAM_CPU_USAGE;
    p->wait_strt_time = get_curr_time();
    
  }
  
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;
  uint xticks;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);

  myproc()->stime = xticks;

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // estimate next cpu burst
  if(p->batch){
    estimate_cpu_burst(p);
    // printf("in sleep, adding 100 to cpu, PID: %d\n", p->pid);
    p->cpu_usage += SCHED_PARAM_CPU_USAGE/2;
  }
  
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;
  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}
// Wake up all processes sleeping on chan.
// Must be called without any p->lock.

void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
        if(p->batch){
          p->wait_strt_time = get_curr_time();
        }
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}

// Print a process listing to console with proper locks held.
// Caution: don't invoke too often; can slow down the machine.
int
ps(void)
{
   static char *states[] = {
  [UNUSED]    "unused",
  [SLEEPING]  "sleep",
  [RUNNABLE]  "runble",
  [RUNNING]   "run",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;
  int ppid, pid;
  uint xticks;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->state == UNUSED) {
      release(&p->lock);
      continue;
    }
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

    pid = p->pid;
    release(&p->lock);
    acquire(&wait_lock);
    if (p->parent) {
       acquire(&p->parent->lock);
       ppid = p->parent->pid;
       release(&p->parent->lock);
    }
    else ppid = -1;
    release(&wait_lock);

    acquire(&tickslock);
    xticks = ticks;
    release(&tickslock);

    printf("pid=%d, ppid=%d, state=%s, cmd=%s, ctime=%d, stime=%d, etime=%d, size=%p", pid, ppid, state, p->name, p->ctime, p->stime, (p->endtime == -1) ? xticks-p->stime : p->endtime-p->stime, p->sz);
    printf("\n");
  }
  return 0;
}

int
pinfo(int pid, uint64 addr)
{
   struct procstat pstat;

   static char *states[] = {
  [UNUSED]    "unused",
  [SLEEPING]  "sleep",
  [RUNNABLE]  "runble",
  [RUNNING]   "run",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;
  uint xticks;
  int found=0;

  if (pid == -1) {
     p = myproc();
     acquire(&p->lock);
     found=1;
  }
  else {
     for(p = proc; p < &proc[NPROC]; p++){
       acquire(&p->lock);
       if((p->state == UNUSED) || (p->pid != pid)) {
         release(&p->lock);
         continue;
       }
       else {
         found=1;
         break;
       }
     }
  }
  if (found) {
     if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
         state = states[p->state];
     else
         state = "???";

     pstat.pid = p->pid;
     release(&p->lock);
     acquire(&wait_lock);
     if (p->parent) {
        acquire(&p->parent->lock);
        pstat.ppid = p->parent->pid;
        release(&p->parent->lock);
     }
     else pstat.ppid = -1;
     release(&wait_lock);

     acquire(&tickslock);
     xticks = ticks;
     release(&tickslock);

     safestrcpy(&pstat.state[0], state, strlen(state)+1);
     safestrcpy(&pstat.command[0], &p->name[0], sizeof(p->name));
     pstat.ctime = p->ctime;
     pstat.stime = p->stime;
     pstat.etime = (p->endtime == -1) ? xticks-p->stime : p->endtime-p->stime;
     pstat.size = p->sz;
     if(copyout(myproc()->pagetable, addr, (char *)&pstat, sizeof(pstat)) < 0) return -1;
     return 0;
  }
  else return -1;
}


// fork a process and set the basepriority of the child process.
int
forkp(int prior)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  np->basepriority = prior;
  np->priority = 0;
  np->batch = 1;
  np->last_cpu_burst = 0;
  np->last_estimate = 0;
  np->cpu_usage = 0;

  if(curr_no_proc == 0){
    curr_no_proc = 1;
    total_no_proc = 1;
    
    batch_start_time = get_curr_time();
    
    total_trnarnd_time = 0;
    
    total_wait_time = 0;
    max_cmpl_time = 0;
    min_cmpl_time = __INT_MAX__;

    total_abs_error = 0;
    num_error = 0;
    
    total_cpu_burst = 0;
    num_cpu_burst = 0;
    max_cpu_burst = 0;
    min_cpu_burst = __INT_MAX__;

    tot_estm_burst = 0;
    num_estm_burst = 0;
    max_estm_burst = 0;
    min_estm_burst = __INT_MAX__;

  }
  else{
    curr_no_proc+=1;
    total_no_proc+=1;

  }
  np->wait_strt_time = get_curr_time();
  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}