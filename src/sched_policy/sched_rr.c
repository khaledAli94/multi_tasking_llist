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

static struct task_t *rr_pick_next(struct task_t *current)
{
    return current->next;
}

static void rr_enqueue(struct task_t *t)
{
    (void)t;
}

static void rr_dequeue(struct task_t *t)
{
    (void)t;
}

static void rr_tick(struct task_t *current)
{
    if (current->preempt_count == 0) {
        current->preempt_count = SCHED_TICK_MS * 10;
        preempt();
        return;
    }
    current->preempt_count--;
}

const struct sched_ops sched_ops_rr = {
    .name      = "round-robin",
    .policy    = SCHED_RR,
    .pick_next = rr_pick_next,
    .enqueue   = rr_enqueue,
    .dequeue   = rr_dequeue,
    .tick      = rr_tick,
};