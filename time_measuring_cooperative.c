/* sched.c */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <cp15_timer.h>
#include <timer_utils.h>
#include <sched.h>
#include <irq.h>
#include "string.h"
#include "malloc.h"

static struct task_t *__tlist = NULL;

void init_sched(void)
{
    cp15_pmu_enable();
    timer_init();

    struct task_t *t = malloc(sizeof(struct task_t));
    t->name = "main_idle";
    t->entry_func = NULL;
    t->stack_base = NULL; 

    t->v_accum = 0;
    t->cycles_used = 0;
    t->yield_count = 0;
    
    t->start_time = cp15_read_cntvct();
    t->cycle_start = cp15_read_pmccntr();
    
    __tlist = t;
    t->next = t;
    t->prev = t;
}

/* cur <=> new */
__attribute__((always_inline))
static inline void append_task(struct task_t *t)
{
    if (__tlist == NULL) {
        // First task becomes the root
        __tlist = t;
        t->next = t;
        t->prev = t;
    } else {
        // Standard circular insertion
        t->next = __tlist->next;
        t->prev = __tlist;
        __tlist->next->prev = t;
        __tlist->next = t;
    }
}


static void task_entry_trampoline(void)
{
    struct task_t *me = __tlist;
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

    t->start_time  = cp15_read_cntvct(); 
    t->cycle_start = cp15_read_pmccntr();

    t->entry_func = func;
    t->user_arg   = arg;

    t->stack_base = stack_mem;
    t->stack_size = stk_sz;


    uint8_t *stack_top = (uint8_t *)stack_mem + stk_sz;
    stack_top = (uint8_t *)((uintptr_t)stack_top & ~7UL);

    stack_top -= sizeof(struct cpu_ctx);
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

void yield(void) {
    struct task_t *curr = __tlist;
    struct task_t *next = curr->next;

    // --- TASK EXIT STAMP (curr) ---
    uint64_t now_vct = cp15_read_cntvct();
    uint32_t now_cyc = cp15_read_pmccntr();

    // Accumulate time spent in 'curr'
    curr->v_accum    += (now_vct - curr->start_time);
    // Note: PMU counter grows upward; result is (current - start)
    curr->cycles_used += (now_cyc - curr->cycle_start); 
    curr->yield_count++;

    // --- CONTEXT SWITCH ---
    __tlist = next; 

    // Update 'next' start stamps before switching
    next->start_time  = now_vct;
    next->cycle_start = now_cyc;

    switch_fcontext(&curr->context, next->context);
}
