/* sched_edf.c */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <cp15_timer.h>
#include <timer_utils.h>
#include <sched.h>
#include <irq.h>
#include "string.h"
#include "malloc.h"

static struct task_t *find_earliest_deadline(struct task_t *head)
{
    struct task_t *t    = head;
    struct task_t *best = head;
    uint64_t       min  = head->deadline;

    do {
        if (t->deadline < min) {
            min  = t->deadline;
            best = t;
        }
        t = t->next;
    } while (t != head);

    return best;
}

static struct task_t *edf_pick_next(struct task_t *current)
{
    return find_earliest_deadline(which_sched()->running);
}

static void edf_enqueue(struct task_t *t)
{
    (void)t;
}

static void edf_dequeue(struct task_t *t)
{
    (void)t;
}

static void edf_tick(struct task_t *current)
{
    struct task_t *best = find_earliest_deadline(which_sched()->running);

    if (best != current && best->deadline < current->deadline) {
        preempt();
    }

    uint64_t now = cp15_read_cntvct();
    if (current->deadline < now) {
        // printf("DEADLINE MISS: task=%s now=%llu deadline=%llu\n", current->name, now, current->deadline);
    }
}

const struct sched_ops sched_ops_edf = {
    .name      = "edf",
    .policy    = SCHED_EDF,
    .pick_next = edf_pick_next,
    .enqueue   = edf_enqueue,
    .dequeue   = edf_dequeue,
    .tick      = edf_tick,
};