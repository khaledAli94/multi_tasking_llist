#ifndef __SCHED_H__
#define __SCHED_H__

#include <stddef.h> // for size_t
#include <stdint.h> // for uint64_t

#ifdef __cplusplus
extern "C" {
#endif

#define SCHED_TICK_MS 1U

struct cpu_ctx {
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t lr;
};

struct task_t {
    const char *name;
    
    // 1. WHERE to resume (the saved registers)
    void *context;      // saved SP pointing to saved register block
    
    // 2. WHAT to run (entry point)
    void (*entry_func)(void *);
    void *user_arg;
    
    // 3. WHERE the registers live (the stack)
    void *stack_base;
    size_t stack_size;
    
    
    uint64_t start_time;
    uint64_t v_accum;

    uint32_t cycle_start;
    uint64_t cycles_used;

    uint64_t vruntime;  /* CFS needs this */
    uint32_t priority; /* priority scheduling needs this */

    uint32_t preempt_count;
    uint32_t yield_count;   

    struct task_t *next;
    struct task_t *prev;
};

enum sched_policy_t {
    SCHED_RR       = 0,
    SCHED_PRIORITY = 1,
    SCHED_CFS      = 2,
    SCHED_EDF      = 3
};

struct sched_ops {
    const char *name;
    enum sched_policy_t policy;

    struct task_t *(*pick_next)(struct task_t *current);
    void(*enqueue)(struct task_t *t);
    void(*dequeue)(struct task_t *t);
    void(*tick)(struct task_t *current);
};

struct scheduler_t {
    struct task_t *running;
    const struct sched_ops *ops;
    enum sched_policy_t policy;
    uint32_t tick_count;
};


// extern void *make_fcontext();
extern void switch_fcontext(void **, void *);


struct scheduler_t *which_sched(void);


void init_sched(enum sched_policy_t policy);
void sched_set_policy(enum sched_policy_t policy);


struct task_t *create_task(const char *name, void *stack_mem, size_t stk_sz, void (*func)(void *), void *arg);

void switch_task(void);

void yield(void);
void preempt(void);
void sched_spin(void);


extern const struct sched_ops sched_ops_cfs;

#ifdef __cplusplus
}
#endif

#endif /* __SCHED_H__ */
