/* sched_rr.c */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <cp15_timer.h>
#include <timer_utils.h>
#include <sched.h>
#include <irq.h>
#include "string.h"
#include "malloc.h"

#define RR_TIMESLICE_TICKS 10

static struct task_t *rr_pick_next(struct task_t *current)
{
    /*
     * timeslice == 0 → slice exhausted → move to next.
     * idle->timeslice = 1 → idle never triggers rotation alone.
     */
    if (current->timeslice == 0)
        return current->next;
    return current;
}

static void rr_enqueue(struct task_t *t)
{
    t->timeslice = RR_TIMESLICE_TICKS;
}

static void rr_dequeue(struct task_t *t)
{
    (void)t;
}

static void rr_tick(struct task_t *current)
{
    if (current->timeslice > 0)
        current->timeslice--;
}

const struct sched_ops sched_ops_rr = {
    .name      = "round-robin",
    .policy    = SCHED_RR,
    .pick_next = rr_pick_next,
    .enqueue   = rr_enqueue,
    .dequeue   = rr_dequeue,
    .tick      = rr_tick,
};