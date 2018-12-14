#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct _ptable {
	struct spinlock lock;
	struct proc proc[NPROC];
} ptable;

typedef int lock_t;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);
extern pte_t * _walkpgdir(pde_t *pgdir, const void *va, int alloc);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
int
zbKiller(struct proc* cp)
{
  struct proc *p;
  struct proc *mthread = proc->m_thread;
  for (int i = 1; i < NPROC; i++) {
    if (mthread->ch_thread[i] && mthread->ch_thread[i]->state == ZOMBIE) {
      p = mthread->ch_thread[i];
      if (p == cp) continue;
      mthread->ch_thread[i] = 0;
      kfree(p->kstack);
      p->kstack = 0;
      p->parent = 0;
      p->killed = 0;
      p->name[0] = 0;
      p->pid = 0;
      p->chan = 0;
      p->isthread = 0;
      p->m_thread = 0;
      p->tnum = 0;
      p->tid = 0;
      p->state = UNUSED;
    }
  }
  return 0;
}

static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->chan = 0;
  p->killed = 0;
  p->priority = 0;
  p->tid = p->pid;
  p->m_thread = p;
  p->tnum = 0;
  p->isthread = 0;
  for (int i = 0; i < 8; i++)
    p->trdOn[i] = 0;
  for (int i = 0; i < NPROC; i++) {
    p->ch_thread[i] = 0;
    p->ret[i] = 0;
    p->ustack[i] = 0;
  }
  p->ch_thread[0] = p;

  release(&ptable.lock);

  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;

}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  
  p = allocproc();
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *mthread = proc->m_thread;

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }

  np->sz = proc->sz;
  np->parent = mthread;               
  *np->tf = *proc->tf;
  for (i = 0; i < NPROC; i++) {     
    np->ustack[i] = mthread->ustack[i];
    if (mthread->ch_thread[i] == proc && i != 0) {
      pte_t *pteo = _walkpgdir(np->pgdir, (void*)np->ustack[i] - PGSIZE, 0);
      uint pao = PTE_ADDR(*pteo);
      pte_t *pten = _walkpgdir(np->pgdir, (void*)np->ustack[0] - PGSIZE, 0);
      uint pan = PTE_ADDR(*pten);
      memmove((char*)P2V(pan), (char*)P2V(pao), PGSIZE);
      np->tf->esp = (np->tf->esp - np->ustack[i]) + np->ustack[0];
    }
  }

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}
// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *mthread = proc->m_thread;
  struct proc *p;
  int fd;
  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for (int i = 0; i < NPROC; i++) {
    if (mthread->ch_thread[i] && mthread->ch_thread[i]->state != ZOMBIE) {
      for(fd = 0; fd < NOFILE; fd++){
        if(mthread->ch_thread[i]->ofile[fd]){
          fileclose(mthread->ch_thread[i]->ofile[fd]);
          mthread->ch_thread[i]->ofile[fd] = 0;
        }
      }
      begin_op();
      iput(mthread->ch_thread[i]->cwd);
      end_op();
      mthread->ch_thread[i]->cwd = 0;
    }
  }


  acquire(&ptable.lock);

  for (int i = 0; i < NPROC; i++) {
    if (mthread->ch_thread[i]) {
      mthread->ch_thread[i]->state = ZOMBIE;
    }
  }
  wakeup1(mthread->parent);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == mthread){
      p->parent = mthread->parent;
      if(p->state == ZOMBIE)
        wakeup1(mthread->parent);
    }
  }
  zbKiller(proc);

  sched();
  panic("zombie exit");
}


// Kill LWP that is zombie.
// Notice that current thread never gets cleaned up from this function.
// ptable.lock must be held before calling

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc->m_thread)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        if (p->m_thread == p) {
          zbKiller(p);
          freevm(p->pgdir);
        }
        else {
          for (int i = 0; i < NPROC; i++) {
            if (p->m_thread->ch_thread[i] == p) {
              p->m_thread->ch_thread[i] = 0;
              break;
            }
          }
        }
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->chan = 0;
	p->isthread = 0;
	p->tid = 0;
	p->m_thread = 0;
	p->tnum = 0;
	p->priority = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    if(!havekids || proc->m_thread->killed){
      release(&ptable.lock);
      return -1;
    }
    sleep(proc->m_thread, &ptable.lock);  
  }
}
//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;

  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot 
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }  
}

int getpid(void){
	return proc->pid;
}

int __test_and_set(lock_t *s, int r){
	__asm__ __volatile__("xchgl %0, %1" : "+m"(*s), "+r"(r));
	return r;
}

void lock_init(lock_t* lock){
	*lock = 0;
}

void lock_acquire(lock_t* lock){
	while(__test_and_set(lock, 1));
}

void lock_release(lock_t *lock){
	*lock = 0;
}

struct proc* _allocproc(void){
	return allocproc();
}

void _wakeup1(void* chan){
	wakeup1(chan);
}
