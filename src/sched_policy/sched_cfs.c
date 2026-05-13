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
    struct task_t *t    = head;
    struct task_t *best = head;
    uint64_t       min  = head->vruntime;

    do {
        if (t->vruntime < min) {
            min  = t->vruntime;
            best = t;
        }
        t = t->next;
    } while (t != head);

    return best;
}

static struct task_t *cfs_pick_next(struct task_t *current)
{
    /* pure selection — no charging here */
    return find_min_vruntime(which_sched()->running);
}

static void cfs_enqueue(struct task_t *t)
{
    /* start new task at current minimum so it runs soon */
    struct task_t *min = find_min_vruntime(which_sched()->running);
    t->vruntime = min->vruntime;
}

static void cfs_dequeue(struct task_t *t)
{
    (void)t;
}

static void cfs_tick(struct task_t *current)
{
    /* charge real elapsed time in counter units (24MHz) */
    uint64_t now   = cp15_read_cntvct();
    uint64_t delta = now - current->start_time;
    current->vruntime += delta;
}

const struct sched_ops sched_ops_cfs = {
    .name      = "cfs",
    .policy    = SCHED_CFS,
    .pick_next = cfs_pick_next,
    .enqueue   = cfs_enqueue,
    .dequeue   = cfs_dequeue,
    .tick      = cfs_tick,
};