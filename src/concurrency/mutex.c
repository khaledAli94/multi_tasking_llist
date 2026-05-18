/* mutex.c */
#include "mutex.h"
#include <sched.h>

/* KERNEL PRIMITIVES
 * Mutex — single CPU, cooperative multitasking.
 * The atomicity comes from disabling IRQs:

 * Rule:
 *   - Never hold a mutex across yield() if another task needs it.
 *   - Never use mutex from inside an IRQ handler.
 *   - yield() inside lock() means CPU is never wasted spinning.
 *
 * Wrong use:
 *   - Multi-CPU: use spinlock instead.
 *   - IRQ context: disable IRQs instead.
 */

void __mutex_init(struct mutex_t *m, const char *name)
{
    m->locked = 0;
    m->name   = name;
}

void mutex_lock(struct mutex_t *m)
{
    unsigned cpsr;

    while (1) {
        /*
         * Disable IRQs for the check+set sequence.
         * On a single CPU this is enough to make it atomic —
         * no other task can run between check and set
         * because IRQ (the only way to preempt) is disabled.
         */
        __asm__ volatile(
            "MRS %0, cpsr\n"
            "CPSID if"
            : "=r"(cpsr) :: "memory"
        );

        if (!m->locked) {
            m->locked = 1;
            /* restore IRQs — we have the lock */
            __asm__ volatile(
                "MSR cpsr_cxsf, %0"
                :: "r"(cpsr) : "memory"
            );
            return;
        }

        /*
         * Lock is taken — restore IRQs BEFORE yield().
         * If we yield with IRQs disabled the scheduler
         * cannot fire timer IRQ → deadlock.
         */
        __asm__ volatile(
            "MSR cpsr_cxsf, %0"
            :: "r"(cpsr) : "memory"
        );

        /*
         * yield() here:
         *   - charges vruntime to us
         *   - CFS picks lock holder (or anyone else)
         *   - lock holder runs, releases mutex
         *   - we get picked again, retry
         *   - zero CPU wasted
         */
        yield();
    }
}

void mutex_unlock(struct mutex_t *m)
{
    __asm__ volatile("dmb" ::: "memory");
    /*
     * Simple store — no atomic needed on single CPU.
     * IRQ cannot fire between our check and store
     * if caller held IRQs off, but normally just store is fine.
     */
    m->locked = 0;
}

int mutex_trylock(struct mutex_t *m)
{
    unsigned cpsr;
    int      got;

    __asm__ volatile(
        "MRS %0, cpsr\n"
        "CPSID if"
        : "=r"(cpsr) :: "memory"
    );

    if (!m->locked) {
        m->locked = 1;
        got = 1;
    } else {
        got = 0;
    }

    __asm__ volatile(
        "MSR cpsr_cxsf, %0"
        :: "r"(cpsr) : "memory"
    );

    return got;
}