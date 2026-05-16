#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct spinlock_t {
    const char *name;
    volatile int locked;
};

#define spinlock_init(n) {.name = (n), .locked = 0}


void __spinlock_init(struct spinlock_t *s, const char *name);
void spinlock_lock(struct spinlock_t *s);
void spinlock_unlock(struct spinlock_t *s);
int  spinlock_trylock(struct spinlock_t *s);   /* 1 = got it, 0 = busy */

#ifdef __cplusplus
}
#endif

#endif /* __SPINLOCK_H__ */