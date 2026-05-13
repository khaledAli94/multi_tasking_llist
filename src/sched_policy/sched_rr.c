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

#define RR_TIMESLICE_TICKS  10   /* 10 timer ticks per slice */

static struct task_t *rr_pick_next(struct task_t *current)
{
    /* RR: always next in ring
     * irq_preempt decides whether to actually switch
     * based on whether timeslice expired (vruntime used as counter)
     */
    if (current->vruntime == 0)
        return current->next;   /* timeslice expired → next task  */
    return current;             /* still has time → stay          */
}

static void rr_enqueue(struct task_t *t)
{
    t->vruntime = RR_TIMESLICE_TICKS;   /* fresh timeslice on entry */
}

static void rr_dequeue(struct task_t *t)
{
    (void)t;
}

static void rr_tick(struct task_t *current)
{
    /* count down timeslice */
    if (current->vruntime > 0)
        current->vruntime--;
    /* when vruntime hits 0, pick_next will return current->next */
}

const struct sched_ops sched_ops_rr = {
    .name      = "round-robin",
    .policy    = SCHED_RR,
    .pick_next = rr_pick_next,
    .enqueue   = rr_enqueue,
    .dequeue   = rr_dequeue,
    .tick      = rr_tick,
};