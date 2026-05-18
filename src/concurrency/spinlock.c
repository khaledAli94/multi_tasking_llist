/* spinlock.c */
#include "spinlock.h"

/*
 * Spinlock — for multi-CPU true parallel access.
 * hardware exclusive monitor for atomicity:
 * 
 * Uses LDREX/STREX — ARMv7 exclusive monitor.
 *
 * Rule:
 *   - Never yield() inside spin — CPU must keep checking.
 *   - Keep critical section SHORT — other CPUs burn cycles waiting.
 *   - Safe to use from IRQ handler (unlike mutex).
 *   - On single CPU this works but is DANGEROUS:
 *       if lock holder gets preempted while lock is held
 *       and waiter spins → deadlock.
 *       Use mutex on single CPU instead.
 *
 * LDREX/STREX explained:
 *   LDREX r0, [addr]  — load + mark address as "exclusive"
 *   STREX r1, r2, [addr] — store r2 to addr ONLY if still exclusive
 *                          r1 = 0 means success
 *                          r1 = 1 means someone else touched it → retry
 */

void __spinlock_init(struct spinlock_t *s, const char *name)
{
    s->locked = 0;
    s->name   = name;
}

void spinlock_lock(struct spinlock_t *s)
{
    unsigned tmp;
    unsigned got;

    __asm__ volatile (
        "1:                          \n"
        "   ldrex  %0, [%2]         \n"  /* tmp = *locked (exclusive)  */
        "   cmp    %0, #0           \n"  /* is it free?                */
        "   bne    1b               \n"  /* no  → spin (try again)     */
        "   mov    %0, #1           \n"  /* yes → prepare value 1      */
        "   strex  %1, %0, [%2]     \n"  /* try to store 1 exclusively */
        "   cmp    %1, #0           \n"  /* did store succeed?         */
        "   bne    1b               \n"  /* no  → someone raced → retry*/
        "   dmb                     \n"  /* memory barrier             */
        : "=&r"(tmp), "=&r"(got)
        : "r"(&s->locked)
        : "memory", "cc"
    );

    /*
     * dmb (Data Memory Barrier):
     * Ensures all memory accesses before the lock
     * are visible before we enter the critical section.
     * Without this, CPU or compiler could reorder
     * memory ops across the lock boundary.
     */
}

void spinlock_unlock(struct spinlock_t *s)
{
    __asm__ volatile (
        "dmb            \n"  /* barrier: finish all work inside CS first */
        "str %1, [%0]   \n"  /* store 0 → release lock                  */
        :: "r"(&s->locked), "r"(0)
        : "memory"
    );
}

int spinlock_trylock(struct spinlock_t *s)
{
    unsigned tmp;
    unsigned got;

    __asm__ volatile (
        "ldrex  %0, [%2]     \n"  /* exclusive load          */
        "cmp    %0, #0       \n"  /* free?                   */
        "movne  %1, #0       \n"  /* no  → got=0             */
        "bne    2f           \n"  /* no  → exit              */
        "mov    %0, #1       \n"  /* yes → value to store    */
        "strex  %1, %0, [%2] \n"  /* try exclusive store     */
        "eor    %1, %1, #1   \n"  /* strex: 0=ok → flip to 1 */
        "dmb                 \n"
        "2:                  \n"
        : "=&r"(tmp), "=&r"(got)
        : "r"(&s->locked)
        : "memory", "cc"
    );

    return got;   /* 1 = got lock, 0 = busy */
}