#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "spinlock.h"

extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

extern struct proc* _allocproc(void);
extern void _wakeup1(void *chan);

int
thread_create(void *(*function)(void *), int priority, void *arg, void *stack)
{
  int i;
  struct proc *np;
  struct proc *mthread = proc->m_thread;   
  uint sp = 0, ustack[3];
  if(mthread->tnum >= 7) return -1;
  if(!(np = _allocproc()))
    return -1;
  
  acquire(&ptable.lock);
  for (i = 0; i < NPROC; i++) {
    if (!mthread->ch_thread[i]) {
      mthread->ch_thread[i] = np;
      sp = (int)stack;
      memset((void*)(sp - PGSIZE), 0, PGSIZE);
      break;
    }
  }
  release(&ptable.lock);

  if (i == NPROC) {
    np->state = UNUSED;
    return -1;
  }
  ustack[0] = 0xffffffff;  
  ustack[1] = (uint)arg;
  ustack[2] = 0;
  sp -= sizeof(stack);
  if(copyout(mthread->pgdir, sp, ustack, sizeof(ustack)) < 0) {
    mthread->ch_thread[i] = 0;
    np->state = UNUSED;
    return -1;
  }



  np->pgdir = mthread->pgdir;            
  np->sz = mthread->sz;                 
  *np->tf = *mthread->tf;              
  np->tf->esp = sp;                   
  
  for(i = 0; i < NOFILE; i++)
    if(mthread->ofile[i])
      np->ofile[i] = filedup(mthread->ofile[i]);
  np->cwd = idup(mthread->cwd);

  safestrcpy(np->name, mthread->name, sizeof(mthread->name));

  np->pid = mthread->pid; 
  for(i = 0; i < 7 && mthread->trdOn[i]; i++);                
  np->tid = i;
  mthread->trdOn[i] = 1;
  np->m_thread = mthread;           
  np->parent = mthread->parent;    
  np->ch_thread[0] = 0;           
  np->tf->eip = (uint)function;  
  np->isthread = 1;
  mthread->tnum++;
  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return np->tid;
}

void
thread_exit(void *retval)
{
  struct proc *mthread = proc->m_thread;

  if (proc == mthread)
    exit();

  for (int i = 1; i < NPROC; i++)
    if (mthread->ch_thread[i] == proc)
      mthread->ret[i] = retval;

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);
  
  proc->state = ZOMBIE;
  _wakeup1(mthread);

  sched();
  
  panic("zombie thread_exit");
}

int
thread_join(int tid, void **retval)
{
  int found = 0, i;
  struct proc *mthread = proc->m_thread;
  struct proc *cproc;

  acquire(&ptable.lock);

  for(;;){
    found = 0;

    for (i = 0; i < NPROC; i++) { 
      if (mthread->ch_thread[i] && mthread->ch_thread[i]->isthread && mthread->ch_thread[i]->tid == tid) {
        cproc = mthread->ch_thread[i];
        found = 1;
        break;
      }
    }

    if (!found) {
      release(&ptable.lock);
      return -1;
    }
//cprintf("joining thread tid: %d status %d\n", tid, cproc->state);
    if (cproc->state == ZOMBIE) {
      if(retval != 0)
        *retval = mthread->ret[i];

      mthread->ch_thread[i] = 0;
      mthread->ret[i] = 0;
      mthread->tnum--;
      mthread->trdOn[cproc->tid] = 0;
      kfree(cproc->kstack);
      cproc->pid = 0;
      cproc->parent = 0;
      cproc->name[0] = 0;
      cproc->killed = 0;
      cproc->state = UNUSED;
      cproc->priority  = 0;
      cproc->tid = 0;
      cproc->m_thread = 0;
      release(&ptable.lock);
      return 0;
    }

//cprintf("proc %d, tid %d, state %d\n",gettid(), tid, cproc->state);
    if(mthread->killed){
      release(&ptable.lock);
      return -1;
    }

    sleep(mthread, &ptable.lock);  
  }
}

int gettid(void){
	return proc->tid;
}
