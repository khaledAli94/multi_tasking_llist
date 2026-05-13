/* exception.c */
#include <stdint.h>
#include <stddef.h>
#include <exception.h>
#include "gic.h"
#include "irq.h"
#include "sched.h"
#include "timer_utils.h"
#include "cp15_timer.h"



uint32_t *irq_handler_c(uint32_t *curr_sp)
{
    uint32_t id = gic_ack_irq();

    if (id != GIC_MAX_IRQS) {
        irq_dispatch(id);
        gic_eoi(id);
    }

    return irq_preempt(curr_sp);
}
