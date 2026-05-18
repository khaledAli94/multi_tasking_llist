/* sched_cfs.c */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <cp15_timer.h>
#include <timer_utils.h>
#include <sched.h>
#include <irq.h>
#include "string.h"
#include "malloc.h"

static struct task_t *find_min_vruntime(struct task_t *head)
{
    struct task_t *t = head;
    struct task_t *best = head;
    uint64_t min = head->vruntime;

    do {
        if (t->vruntime < min) {
            min  = t->vruntime;
            best = t;
        }
        t = t->next;
    } while (t != head);

    return best;
    /*
     * idle->vruntime = UINT64_MAX — always loses.
     * No idle check needed.
     */
}

static struct task_t *cfs_pick_next(struct task_t *current)
{
    return find_min_vruntime(current);
}

static void cfs_enqueue(struct task_t *t)
{
    /*
     * Join at watermark — clean, no ring search, no idle check.
     */
    t->vruntime = which_sched()->min_vruntime;
}

static void cfs_dequeue(struct task_t *t)
{
    (void)t;
}

static void cfs_tick(struct task_t *current)
{
    /*
     * current->start_time was set when this task got the cpu.
     * yield/irq_preempt reset start_time AFTER calling tick
     * so delta is valid here.
     */
    uint64_t now   = cp15_read_cntvct();
    uint64_t delta = now - current->start_time;

    current->vruntime += delta;

    /* watermark only moves forward */
    struct scheduler_t *sched = which_sched();
    if (current->vruntime > sched->min_vruntime)
        sched->min_vruntime = current->vruntime;
}

const struct sched_ops sched_ops_cfs = {
    .name      = "cfs",
    .policy    = SCHED_CFS,
    .pick_next = cfs_pick_next,
    .enqueue   = cfs_enqueue,
    .dequeue   = cfs_dequeue,
    .tick      = cfs_tick,
};