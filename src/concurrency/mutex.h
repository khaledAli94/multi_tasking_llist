#ifndef __MUTEX_H__
#define __MUTEX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * Mutex — single CPU, multi-task
 * Waiting task calls yield() → scheduler runs others
 * Lock holder eventually runs → releases → waiter gets it
 *
 * USE FOR: uart, shared data between tasks, any slow resource
 */
struct mutex_t {
    volatile int locked;
    const char  *name;       /* debug only */
};

#define mutex_init(n) {.name = (n), .locked = 0}

void __mutex_init(struct mutex_t *m, const char *name);
void mutex_lock(struct mutex_t *m);
void mutex_unlock(struct mutex_t *m);
int  mutex_trylock(struct mutex_t *m);   /* non-blocking, returns 1=got it 0=busy */

#ifdef __cplusplus
}
#endif

#endif /* __MUTEX_H__ */