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

static uint64_t sched_account_out(struct task_t *curr)
{
    uint64_t now_vct  = cp15_read_cntvct();
    uint32_t now_cyc  = cp15_read_pmccntr();

    curr->v_accum     += (now_vct - curr->start_time);
    curr->cycles_used += (uint32_t)(now_cyc - curr->cycle_start);

    return now_vct;
}

/*
 * sched_account_in - stamp a task that is GETTING the cpu.
 *
 * Call with IRQs disabled.
 */
static void sched_account_in(struct task_t *next, uint64_t now_vct)
{
    uint32_t now_cyc   = cp15_read_pmccntr();
    next->start_time   = now_vct;
    next->cycle_start  = now_cyc;
}

struct task_t *create_task(const char *name, void *stack_mem, size_t stk_sz, void (*func)(void *), void *arg)
{
    unsigned irq_state;
    __asm__ volatile("MRS %0, cpsr\n CPSID if" : "=r"(irq_state) :: "memory");

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

    stack_top -= sizeof(struct cpu_ctx_t); // reserve 36 byte
    struct cpu_ctx_t *frame = (struct cpu_ctx_t *)stack_top;

    frame->r0   = 0xdeadbeef;
    frame->r1   = 0xdeadbeef;
    frame->r2   = 0xdeadbeef;
    frame->r3   = 0xdeadbeef;
    frame->r4   = 0xdeadbeef;
    frame->r5   = 0xdeadbeef;
    frame->r6   = 0xdeadbeef;
    frame->r7   = 0xdeadbeef;
    frame->r8   = 0xdeadbeef;
    frame->r9   = 0xdeadbeef;
    frame->r10  = 0xdeadbeef;
    frame->r11  = 0xdeadbeef;
    frame->r12  = 0xdeadbeef;
    frame->lr   = (uint32_t)task_entry_trampoline;
    frame->pc   = (uint32_t)task_entry_trampoline;
    frame->cpsr = 0x13;             /* SVC mode, IRQs enabled */

    t->context = stack_top;

    append_task(t);
    __asm__ volatile("MSR cpsr_cxsf, %0" :: "r"(irq_state) : "memory");
    return t;
}
