/* extern void switch_fcontext(*,*); */
.globl switch_fcontext
.type switch_fcontext, %function
switch_fcontext:
    /* save cpsr and pc onto stack in rfeia-compatible order */
    mrs     r2,  cpsr
    str     r2,  [sp, #-4]!     /* push cpsr                      */
    str     lr,  [sp, #-4]!     /* push pc (= lr = return addr)   */

    /* save all registers */
    push    {r0-r12, lr}        /* r0-r12, lr                     */

    /* save current context pointer */
    str     sp,  [r0]           /* curr->context = sp             */

    /* switch to next task */
    mov     sp,  r1             /* sp = next->context             */

    /* restore */
    pop     {r0-r12, lr}        /* restore r0-r12, lr             */
    rfeia   sp!                 /* restore pc and cpsr            */