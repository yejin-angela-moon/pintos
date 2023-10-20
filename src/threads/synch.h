#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>


/* A counting semaphore. */
struct semaphore
{
    unsigned value;             /* Current value. */
    struct list waiters;        /* List of waiting threads. */
};
bool list_contains (struct list *list, struct list_elem *elem_to_find);
//void set_donated_priority (struct thread *thr);

void sema_init (struct semaphore *, unsigned value);
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);
void sema_self_test (void);
bool sema_priority(const struct list_elem *fir, const struct list_elem *sec, void *UNUSED);

/* Lock. */
struct lock
{
    struct thread *holder;      /* Thread holding lock (for debugging). */
    struct semaphore semaphore; /* Binary semaphore controlling access. */
    int priority;
};

void lock_init (struct lock *);
void modify_nest_donation (struct thread *thread, int pri);
void lock_acquire (struct lock *);
bool lock_try_acquire (struct lock *);
void lock_release (struct lock *);
bool lock_held_by_current_thread (const struct lock *);

/* Condition variable. */
struct condition
{
    struct list waiters;        /* List of waiting semaphore_elems. */
};

void cond_init (struct condition *);
void cond_wait (struct condition *, struct lock *);
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);

void set_donated_priority(struct thread *);

#define MAX(X, Y) (((X) >= (Y)) ? (X) : (Y))

/* Optimization barrier.

   The compiler will not reorder operations across an
   optimization barrier.  See "Optimization Barriers" in the
   reference guide for more information.*/
#define barrier() asm volatile ("" : : : "memory")

#endif /* threads/synch.h */
