/* task.c */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <cp15_timer.h>
#include <timer_utils.h>
#include <sched.h>
#include <irq.h>
#include "string.h"
#include "malloc.h"

/* cur <=> new */
__attribute__((always_inline))
static inline void append_task(struct task_t *t)
{
    struct scheduler_t *sched = which_sched();
    
    if (sched->running == NULL) {
        // First task becomes the root
        t->next = t;
        t->prev = t;
        sched->running = t;
    } else {
        struct task_t *curr = sched->running;
        // Standard circular insertion
        t->next = curr->next;
        t->prev = curr;
        curr->next->prev = t;
        curr->next = t;
    }

    /* tell policy a new task entered the run queue */
    if (sched->ops && sched->ops->enqueue) {
        sched->ops->enqueue(t);
    }
}


static void task_entry_trampoline(void)
{
    struct task_t *me = which_sched()->running;

    while(1) {
        me->entry_func(me->user_arg);
        yield(); 
    }
}

struct task_t *create_task(const char *name, void *stack_mem, size_t stk_sz, void (*func)(void *), void *arg)
{
    struct task_t *t = malloc(sizeof(struct task_t));

    t->name       = strdup(name);

    t->v_accum     = 0;
    t->cycles_used = 0;
    t->yield_count = 0;
    t->preempt_count = 0;

    t->priority      = 128;              /* default mid priority */
    t->vruntime      = 0;               /* cfs_enqueue will set this */

    t->start_time  = cp15_read_cntvct(); 
    t->cycle_start = cp15_read_pmccntr();

    t->entry_func = func;
    t->user_arg   = arg;

    t->stack_base = stack_mem;
    t->stack_size = stk_sz;


    uint8_t *stack_top = (uint8_t *)stack_mem + stk_sz;
    stack_top = (uint8_t *)((uintptr_t)stack_top & ~7UL);

    stack_top -= sizeof(struct cpu_ctx); // reserve 36 byte
    struct cpu_ctx *frame = (struct cpu_ctx *)stack_top;

    frame->r4 = 0xdeadbeef;
    frame->r5 = 0xdeadbeef;
    frame->r6 = 0xdeadbeef;
    frame->r7 = 0xdeadbeef;
    frame->r8 = 0xdeadbeef;
    frame->r9 = 0xdeadbeef;
    frame->r10 = 0xdeadbeef;
    frame->r11 = 0xdeadbeef;
    frame->lr = (uint32_t)task_entry_trampoline;


    t->context = stack_top;

    append_task(t);
    return t;
}
