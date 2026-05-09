
struct task_t {
    const char *name;
    
    // 1. WHERE to resume (the saved registers)
    void *context;      // saved SP pointing to saved register block
    
    // 2. WHAT to run (entry point)
    void (*entry_func)(void *);
    void *user_arg;
    
    // 3. WHERE the registers live (the stack)
    void *stack_base;
    size_t stack_size;
    
    struct task_t *next;
    struct task_t *prev;
};


struct task_pdat_t {
    char *text;
    unsigned val;
};

static struct task_t *__tlist;

static struct task_pdat_t t1_pdat = {.text = "hi there task1", .val = 1};
static struct task_pdat_t t2_pdat = {.text = "hi there task2", .val = 2};


void task1(void *arg)
{
    struct task_pdat_t *pdat = arg;
    pdat->val++;
    for (volatile int i = 0; i < 10; i++);
}

void task2(void *arg)
{
    struct task_pdat_t *pdat = arg;
    pdat->val++;
    for (volatile int i = 0; i < 20; i++);
}

struct task_t *create_task(const char *name, void *stack_top, size_t stk_sz, void (*func)(void *), void *arg)
{
    struct task_t *t = malloc(sizeof(struct task_t));

    t->name       = strdup(name);
    t->stack_base = (char*)stack_top + stk_sz;   // FIXED
    t->stack_size = stk_sz;
    t->entry_func = func;
    t->user_arg   = arg;

    // circular self-link
    t->next = t;
    t->prev = t;

    return t;
}

void switch_task(void)
{
    // run current task
    __tlist->entry_func(__tlist->user_arg);

    // move to next task
    __tlist = __tlist->next;
}

int main(void)
{
    void *stk1 = malloc(2048);
    void *stk2 = malloc(2048);

    struct task_t *t1 = create_task("task1", stk1, 2048, task1, &t1_pdat);
    struct task_t *t2 = create_task("task2", stk2, 2048, task2, &t2_pdat);

    // link tasks into a ring
    t1->next = t2;
    t2->next = t1;

    t1->prev = t2;
    t2->prev = t1;

    __tlist = t1;   // start scheduler

    while (1) switch_task();

    return 0;
}
