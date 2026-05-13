/* main.c */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "string.h"
#include <cp15_timer.h>
#include <timer_utils.h>
#include "malloc.h"
#include <exception.h>
#include <sched.h>

struct task_pdat_t {
    char *text;
    unsigned val;
    float fval;
};


static struct task_pdat_t t1_pdat = {.text = "hi there task1", .val = 1};
static struct task_pdat_t t2_pdat = {.text = "hi there task2", .val = 2};
static struct task_pdat_t t3_pdat = {.text = "hi there task3", .val = 3};

void task1(void *arg)
{
    struct task_pdat_t *pdat = arg;
    while(1) {
        pdat->val++;
        pdat->fval +=0.1f;
        yield();  
    }
}

void task2(void *arg)
{
    struct task_pdat_t *pdat = arg;
    while(1) {
        pdat->val++;
        pdat->fval +=0.2f;
    }
}

void task3(void *arg)
{
    struct task_pdat_t *pdat = arg;
    while(1) {
        pdat->val++;
    }
}


int main(void)
{
    init_sched(SCHED_CFS);

    void *stk1 = malloc(512);
    void *stk2 = malloc(512);
    void *stk3 = malloc(512);

    struct task_t *t1 = create_task("task1", stk1, 512, task1, &t1_pdat);
    struct task_t *t2 = create_task("task2", stk2, 512, task2, &t2_pdat);
    struct task_t *t3 = create_task("task3", stk3, 512, task3, &t3_pdat);

    sched_spin();

    return 0;
}
