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


struct task_t *create_task(const char *name, void *stack_top, size_t stk_sz, void (*func)(void *), void *arg)
{
    struct task_t *t = malloc(sizeof(struct task_t));

    t->name       = strdup(name);
    t->entry_func = func;
    t->user_arg   = arg;

    t->stack_base = (char*)((uintptr_t)(stack_top) & ~7U) + stk_sz - sizeof(struct cpu_ctx );
    t->stack_size = stk_sz;

    t->context = make_fcontext(t->stack_base);

   append_task(t);
    return t;
}

void switch_task(void)
{
    // run current task
    __tlist->entry_func(__tlist->user_arg);

    // move to next task
    __tlist = __tlist->next;
}

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
    // for(;;);
}

void task2(void *arg)
{
    struct task_pdat_t *pdat = arg;
    pdat->val++;
    int x = 3;
    int y = 4;
    int z = x*y;
    // for(;;);
}


int main(void)
{
    void *stk1 = malloc(2048);
    void *stk2 = malloc(2048);

    struct task_t *t1 = create_task("task1", stk1, 2048, task1, &t1_pdat);
    struct task_t *t2 = create_task("task2", stk2, 2048, task2, &t2_pdat);


    while (1) switch_task();

    return 0;
}
