// lock.h - A sleep lock
//

#ifdef LOCK_TRACE
#define TRACE
#endif

#ifdef LOCK_DEBUG
#define DEBUG
#endif

#ifndef _LOCK_H_
#define _LOCK_H_

#include "thread.h"
#include "halt.h"
#include "console.h"

#define UNLOCK 1

struct lock {
    struct condition cond;
    int tid; // thread holding lock or -1
};

static inline void lock_init(struct lock * lk, const char * name);
static inline void lock_acquire(struct lock * lk);
static inline void lock_release(struct lock * lk);

// INLINE FUNCTION DEFINITIONS
//

static inline void lock_init(struct lock * lk, const char * name) {
    trace("%s(<%s:%p>", __func__, name, lk);
    condition_init(&lk->cond, name);
    lk->tid = -1;
}

/*
static inline void lock_acquire(struct lock * lk)
Inputs:
lk - The lock that we are acquiring to use to do the  locking

Outputs:
None

Effects:
lock_acquire gives us a lock and attatches it to the running_thread, waiting if the lock is currently being used

Description:
lock_acquire allows us to use a lock in a device or file. We take in a lock, and if the lock is currently unlocked (not being used),
we can set the tid of the lock to the current thread. If the lock is being used, we have to use condition_wait to wait until
the lock's current condition has been met. After it has been, we can now use this lock for the current thread.
*/

static inline void lock_acquire(struct lock * lk) {
    // TODO: FIXME implement this

    // if lock is unlocked ,change state to locked
    if(lk -> tid == -UNLOCK)
    {
        lk -> tid = running_thread();
    }
    // if lock is locked, suspend until it acquires lock
    else
    {
        condition_wait(&lk -> cond);
        lk -> tid = running_thread();
    }

    // Still need to implement locks in kfs/vioblk drivers,look at mp doc
}

static inline void lock_release(struct lock * lk) {
    trace("%s(<%s:%p>", __func__, lk->cond.name, lk);

    // assert (lk->tid == running_thread());
    
    lk->tid = -1;
    condition_broadcast(&lk->cond);

    debug("Thread <%s:%d> released lock <%s:%p>",
        thread_name(running_thread()), running_thread(),
        lk->cond.name, lk);
}

#endif // _LOCK_H_