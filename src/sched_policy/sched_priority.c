/* sched_priority.c */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <cp15_timer.h>
#include <timer_utils.h>
#include <sched.h>
#include <irq.h>
#include "string.h"
#include "malloc.h"

static struct task_t *find_highest_priority(struct task_t *head)
{
    struct task_t *t    = head;
    struct task_t *best = head;
    uint32_t       min  = head->priority;

    do {
        if (t->priority < min) {
            min  = t->priority;
            best = t;
        }
        t = t->next;
    } while (t != head);

    return best;
}

static struct task_t *priority_pick_next(struct task_t *current)
{
    return find_highest_priority(which_sched()->running);
}

static void priority_enqueue(struct task_t *t)
{
    (void)t;
}

static void priority_dequeue(struct task_t *t)
{
    (void)t;
}

static void priority_tick(struct task_t *current)
{
    struct task_t *best = find_highest_priority(which_sched()->running);

    if (best->priority < current->priority) {
        preempt();
    }
}

const struct sched_ops sched_ops_priority = {
    .name      = "fixed-priority",
    .policy    = SCHED_PRIORITY,
    .pick_next = priority_pick_next,
    .enqueue   = priority_enqueue,
    .dequeue   = priority_dequeue,
    .tick      = priority_tick,
};