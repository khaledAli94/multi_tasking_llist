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

static struct scheduler_t *__sched = NULL;

static const struct sched_ops *policy_table[] = {
    [SCHED_RR]       = &sched_ops_rr,
    [SCHED_PRIORITY] = &sched_ops_priority,
    [SCHED_CFS]      = &sched_ops_cfs,
    [SCHED_EDF]      = &sched_ops_edf,
};

#define NUM_POLICIES (sizeof(policy_table) / sizeof(policy_table[0]))

static struct irq_node sched_irq = {
    .name = "sched_irq",
    .handler = sched_tick,
    .next = NULL,
    .prv = NULL
};

struct scheduler_t *which_sched(void)
{
    return __sched;
}


void init_sched(enum sched_policy_t policy)
{
    cp15_pmu_enable();
    timer_init();
    
    __sched = malloc(sizeof(struct scheduler_t));
    __sched->tick_count = 0;

    struct task_t *idle = malloc(sizeof(struct task_t));

    idle->name = "idle";
    idle->entry_func = NULL;
    idle->user_arg = NULL;
    idle->stack_base = NULL; 
    idle->stack_size = 0; 

    idle->v_accum = 0;
    idle->cycles_used = 0;
    idle->yield_count = 0;
    idle->preempt_count  = 0;

    idle->priority = 255;
    idle->vruntime = 0;
    idle->priority = 20;


    idle->start_time = cp15_read_cntvct();
    idle->cycle_start = cp15_read_pmccntr();

    idle->next = idle;
    idle->prev = idle;
    __sched->running = idle;

    sched_set_policy(policy);

    /* preemptoin support*/
    gic_init();
    irq_register(IRQ_VIRT_TIMER, &sched_irq);
    timer_irq_init(SCHED_TICK_MS);
}

void sched_set_policy(enum sched_policy_t policy)
{
    if ((unsigned)policy >= NUM_POLICIES) {
        // printf("sched: unknown policy %d\n", policy);
        return;
    }
    __sched->policy = policy;
    __sched->ops    = policy_table[policy];
    // printf("sched: policy -> %s\n", __sched->ops->name);
}

void yield(void)
{
    unsigned irq_state;
    __asm__ volatile("MRS %0, cpsr\n CPSID if" : "=r"(irq_state) :: "memory");

    struct task_t *curr = __sched->running;

    /* charge vruntime before picking next */
    uint64_t now_vct = cp15_read_cntvct();
    uint32_t now_cyc = cp15_read_pmccntr();

    uint64_t delta = now_vct - curr->start_time;
    curr->vruntime    += delta;                          /* charge CFS   */
    curr->v_accum     += delta;                          /* accounting   */
    curr->cycles_used += (uint32_t)(now_cyc - curr->cycle_start);
    curr->yield_count++;

    struct task_t *next = __sched->ops->pick_next(curr);

    if (next == curr) {
        curr->start_time  = now_vct;
        curr->cycle_start = now_cyc;
        __asm__ volatile("MSR cpsr_cxsf, %0" :: "r"(irq_state) : "memory");
        return;
    }

    __sched->running  = next;
    next->start_time  = now_vct;
    next->cycle_start = now_cyc;

    // __asm__ volatile("MSR cpsr_cxsf, %0" :: "r"(irq_state) : "memory");
    switch_fcontext(&curr->context, next->context);
}

uint32_t *irq_preempt(uint32_t *curr_sp)
{
    struct scheduler_t *sched = which_sched();
    struct task_t      *curr  = sched->running;

    curr->context = curr_sp;

    /* tick updates vruntime */
    if (sched->ops && sched->ops->tick)
        sched->ops->tick(curr);

    /* now pick best task */
    struct task_t *next = sched->ops->pick_next(curr);

    if (next == curr)
        return curr_sp;

    uint64_t now     = cp15_read_cntvct();
    uint32_t now_cyc = cp15_read_pmccntr();

    curr->v_accum     += now - curr->start_time;
    curr->cycles_used += now_cyc - curr->cycle_start;
    curr->preempt_count++;

    sched->running    = next;
    next->start_time  = now;
    next->cycle_start = now_cyc;

    return next->context;
}

void sched_tick(void *args)
{
    __sched->tick_count++;
    timer_irq_reload(SCHED_TICK_MS);
}

void sched_spin(void)
{
    __asm__ volatile("cpsie i");
    while(1) {
        __asm__ volatile("wfi");   /* sleep until IRQ fires */
        // yield();
    }
}
 