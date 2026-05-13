.global irq
irq:
    sub     lr, lr, #4
    srsdb   sp!, #0x13
    cps     #0x13
    push    {r0-r12, lr}
    mov     r0, sp
    bl      irq_handler_c
    mov     sp, r0
    pop     {r0-r12, lr}
    rfeia   sp!

.global fiq
fiq:
    sub     lr, lr, #4
    srsdb   sp!, #0x13
    cps     #0x13
    push    {r0-r12, lr}
    mov     r0, sp
    bl      fiq_handler_c
    b       .