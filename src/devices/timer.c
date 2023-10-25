#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "devices/pit.h"
#include "threads/interrupt.h"

//#include "threads/thread.h"
  
/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);
static void real_time_delay (int64_t num, int32_t denom);

static list_less_func thread_less_ticks;

/* The list of sleeping threads */
static struct list sleep_list;


/* Sets up the timer to interrupt TIMER_FREQ times per second,
   and registers the corresponding interrupt. */
void
timer_init (void) 
{
  pit_configure_channel (0, 2, TIMER_FREQ);
  intr_register_ext (0x20, timer_interrupt, "8254 Timer");

  list_init(&sleep_list);
}

/* Calibrates loops_per_tick, used to implement brief delays. */
void
timer_calibrate (void) 
{
  unsigned high_bit, test_bit;

  ASSERT (intr_get_level () == INTR_ON);
  printf ("Calibrating timer...  ");

  /* Approximate loops_per_tick as the largest power-of-two
     still less than one timer tick. */
  loops_per_tick = 1u << 10;
  while (!too_many_loops (loops_per_tick << 1)) 
    {
      loops_per_tick <<= 1;
      ASSERT (loops_per_tick != 0);
    }

  /* Refine the next 8 bits of loops_per_tick. */
  high_bit = loops_per_tick;
  for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
    if (!too_many_loops (high_bit | test_bit))
      loops_per_tick |= test_bit;

  printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
int64_t
timer_ticks (void) 
{
  enum intr_level old_level = intr_disable ();
  int64_t t = ticks;
  intr_set_level (old_level);
  return t;
}

/* Helper function to compare wake-up times of two threads in list. */
static bool thread_less_ticks(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
  struct thread_sleep *t_a = list_entry(a, struct thread_sleep, sleep_elem);
  struct thread_sleep *t_b = list_entry(b, struct thread_sleep, sleep_elem);
  return t_a->wake_up_tick < t_b->wake_up_tick;
}


/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
int64_t
timer_elapsed (int64_t then) 
{
  return timer_ticks () - then;
}

/* Sleeps for approximately TICKS timer ticks.  Interrupts must
   be turned on. */
void
timer_sleep (int64_t ticks) 
{
  /* Retrieves the current timer tick. */
  int64_t start = timer_ticks();
  struct thread_sleep t;
  enum intr_level old_level;

  /* Asserts the interrupt is on */
  ASSERT(intr_get_level() == INTR_ON);

  /* Sets up the members of the wrapper thread_sleep */
  t.thread = thread_current();
  sema_init(&t.sleep_wait, 0);
  t.wake_up_tick = start + ticks;

  /* Disables interrupts when inserting the thread into the sleep_list,
  in case multiple threads and the interrupt handler attempt to access it. */
  old_level = intr_disable();

  /* Inserts the thread into the sleep_list, in an ascending order of the
  wake_up_tick value */
  list_insert_ordered(&sleep_list, &t.sleep_elem, &thread_less_ticks, NULL);

  intr_set_level(old_level);

  /* Puts the thread to sleep for TICKS timer ticks, until wake_up_tick is
  reached and timer interrupt handler wakes the thread up. */
  sema_down(&t.sleep_wait);


  ASSERT (intr_get_level () == INTR_ON);   ////??remove
  while (timer_elapsed (start) < ticks) 
    thread_yield ();
}

/* Sleeps for approximately MS milliseconds.  Interrupts must be
   turned on. */
void
timer_msleep (int64_t ms) 
{
  real_time_sleep (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts must be
   turned on. */
void
timer_usleep (int64_t us) 
{
  real_time_sleep (us, 1000 * 1000);
}

/* Sleeps for approximately NS nanoseconds.  Interrupts must be
   turned on. */
void
timer_nsleep (int64_t ns) 
{
  real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Busy-waits for approximately MS milliseconds.  Interrupts need
   not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_msleep()
   instead if interrupts are enabled. */
void
timer_mdelay (int64_t ms) 
{
  real_time_delay (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts need not
   be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_usleep()
   instead if interrupts are enabled. */
void
timer_udelay (int64_t us) 
{
  real_time_delay (us, 1000 * 1000);
}

/* Sleeps execution for approximately NS nanoseconds.  Interrupts
   need not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_nsleep()
   instead if interrupts are enabled.*/
void
timer_ndelay (int64_t ns) 
{
  real_time_delay (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void
timer_print_stats (void) 
{
  printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* Timer interrupt handler. */
static void
timer_interrupt (struct intr_frame *args UNUSED)
{
/* Update the CPU usage for running process. */
  ticks++;
  if (!list_empty(&sleep_list)) {
    /* Retrieves the head of the sleep_list. Since the list is sorted in an
    ascending order based on wake_up_tick, the head thread should have the
    smallest wake_up_tick value: closest to wake up. */
    struct list_elem *e = list_begin(&sleep_list);
    ASSERT(e != NULL);
    struct thread_sleep *t = list_entry(e, struct thread_sleep, sleep_elem);
    ASSERT(t != NULL && t->thread != NULL);

    /* Iteration over threads in sleep_list to wake up every thread
    that has reached the wake up time */
    while (t->wake_up_tick <= ticks) {
      /* Remove the thread that has to wake up */
      list_pop_front(&sleep_list);
      /* "UP" the semaphore for the thread to unblock it and reschedule it
      by adding it to the ready_list */
      sema_up(&t->sleep_wait);

      /* Stop the iteration when the list is completely exhausted */
      if (list_empty(&sleep_list)) {
        break;
      }

      /* The next element to be checked */
      e = list_begin(&sleep_list);
      ASSERT(e != NULL);
      t = list_entry(e, struct thread_sleep, sleep_elem);
      ASSERT(t != NULL && t->thread != NULL);

    }
  }

  thread_tick ();
//struct thread *t = thread_current();
/*if (thread_mlfqs) {
//        printf("mlfqs time\n");
    //if (t != idle_thread)
     // fp_add_int(t->recent_cpu, 1);
    
  if (thread_current()->status == THREAD_RUNNING) {
       // printf("increase cpu by 1\n");
   thread_current()->recent_cpu = fp_add_int(thread_current()->recent_cpu, 1);
 //  printf("new cpu of cur = 0?: %d\n\n\n", thread_current()->recent_cpu == int_to_fp(0));
  }

//	  printf("thread tick %lld and timer freq %d\n", ticks, TIMER_FREQ);
    if (ticks % TIMER_FREQ == 0)
    {
  // printf("now recalculate cpu and la\n");
      recalculate_recent_cpu();
      recalculate_load_avg();
    }
    if (ticks % 4 == 0){
	    calculate_priority_all();
     // struct list_elem *e;  // TODO do not 'wastefully' calc all of them: if you have to, need to replace loop so its not using read_list
     // for (e = list_begin(&ready_list); e != list_end(&ready_list); e = list_next(e))
      //{
       // calculate_priority(list_entry(e, struct thread, elem));
      //}
    }
  }*/
     /* struct thread *cur;
 
        }*/
/*	    printf("thread tick %lld and timer freq %d\n", ticks, TIMER_FREQ);
      if (ticks % TIMER_FREQ == 0)
        {
		printf("now recalculate cpu and la\n");
//		printf("old la: %d\n", fp_to_int_round_nearest(load_avg));
          recalculate_load_avg ();

 
    }*/
}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops (unsigned loops) 
{
  /* Wait for a timer tick. */
  int64_t start = ticks;
  while (ticks == start)
    barrier ();

  /* Run LOOPS loops. */
  start = ticks;
  busy_wait (loops);

  /* If the tick count changed, we iterated too long. */
  barrier ();
  return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait (int64_t loops) 
{
  while (loops-- > 0)
    barrier ();
}

/* Sleep for approximately NUM/DENOM seconds. */
static void
real_time_sleep (int64_t num, int32_t denom) 
{
  /* Convert NUM/DENOM seconds into timer ticks, rounding down.
          
        (NUM / DENOM) s          
     ---------------------- = NUM * TIMER_FREQ / DENOM ticks. 
     1 s / TIMER_FREQ ticks
  */
  int64_t ticks = num * TIMER_FREQ / denom;

  ASSERT (intr_get_level () == INTR_ON);
  if (ticks > 0)
    {
      /* We're waiting for at least one full timer tick.  Use
         timer_sleep() because it will yield the CPU to other
         processes. */                
      timer_sleep (ticks); 
    }
  else 
    {
      /* Otherwise, use a busy-wait loop for more accurate
         sub-tick timing. */
      real_time_delay (num, denom); 
    }
}

/* Busy-wait for approximately NUM/DENOM seconds. */
static void
real_time_delay (int64_t num, int32_t denom)
{
  /* Scale the numerator and denominator down by 1000 to avoid
     the possibility of overflow. */
  ASSERT (denom % 1000 == 0);
  busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000)); 
}
