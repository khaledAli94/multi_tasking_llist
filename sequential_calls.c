/* main.c */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <cp15_timer.h>
#include <timer_utils.h>
#include <sched.h>
#include "malloc.h"
#include <exception.h>
#include "string.h"

struct task_pdat_t {
    char *text;
    unsigned val;
};


static struct task_pdat_t t1_pdat = {.text = "hi there task1", .val = 1};
static struct task_pdat_t t2_pdat = {.text = "hi there task2", .val = 2};


void task1(void *arg)
{
    struct task_pdat_t *pdat = arg;
    pdat->val++;

    int x = 1;
    int y = 2;
    int z = x+y;
}

void task2(void *arg)
{
    struct task_pdat_t *pdat = arg;
    pdat->val++;

    int x = 2;
    int y = 3;
    int z = x*y;
}


int main(void)
{
    // init_sched();

    void *stk1 = malloc(2048);
    void *stk2 = malloc(2048);

    struct task_t *t1 = create_task("task1", stk1, 2048, task1, &t1_pdat);
    struct task_t *t2 = create_task("task2", stk2, 2048, task2, &t2_pdat);


    while (1) yield();

    return 0;
}

/* sched.c */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <cp15_timer.h>
#include <timer_utils.h>
#include <sched.h>
#include <irq.h>
#include <string.h>
#include "malloc.h"

static struct task_t *__tlist = NULL;

struct task_t *get_task_root(void)
{
    return __tlist;
}

void init_sched(void)
{
    struct task_t *t = malloc(sizeof(struct task_t));
    t->name = "main_idle";
    t->entry_func = NULL;
    t->stack_base = NULL; 
    
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
    
    __tlist = next; 
    
    switch_fcontext(&curr->context, next->context);
}
