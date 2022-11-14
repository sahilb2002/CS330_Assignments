#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "condvar.h"



void cond_wait (struct cond_t *cv, struct sleeplock *lock) {
    acquiresleep(&cv->lk);
    condsleep(cv, lock);
}

void cond_signal (struct cond_t *cv) {
    acquiresleep(&cv->lk);
    wakeupone(cv);
    releasesleep(&cv->lk);
}

void cond_broadcast (struct cond_t *cv) {
    acquiresleep(&cv->lk);
    wakeup(cv);
    releasesleep(&cv->lk);
}