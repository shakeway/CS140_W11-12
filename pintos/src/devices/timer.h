#ifndef DEVICES_TIMER_H
#define DEVICES_TIMER_H

#include <round.h>
#include <stdint.h>
#include <list.h>
#include "threads/synch.h"
#include "threads/malloc.h"

/* Number of timer interrupts per second. */
#define TIMER_FREQ 100

void timer_init (void);
void timer_calibrate (void);

int64_t timer_ticks (void);
int64_t timer_elapsed (int64_t);

/* Struct for use in putting threads to sleep.  Each struct contains a
   semaphore which is downed by the thread, putting the thread to sleep
   until time alarm_tick.  The timer_interrupt function checks at each
   tick whether an alarm needs to go off.  If so, it goes through
   and ups those alarms, waking the thread for the scheduler. */
struct alarm {
  struct list_elem elem;
  int64_t alarm_tick;    /* Wait until this tick to wake */
  struct semaphore sem;
};

struct list alarm_list;

/* Sleep and yield the CPU to other threads. */
void timer_sleep (int64_t ticks);
void timer_msleep (int64_t milliseconds);
void timer_usleep (int64_t microseconds);
void timer_nsleep (int64_t nanoseconds);

/* Busy waits. */
void timer_mdelay (int64_t milliseconds);
void timer_udelay (int64_t microseconds);
void timer_ndelay (int64_t nanoseconds);

void timer_print_stats (void);

#endif /* devices/timer.h */
