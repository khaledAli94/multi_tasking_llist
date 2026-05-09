# Circular Doubly-Linked List Task Control API

## Core Operations (O(1))

These operations handle the low-level pointer manipulation required to maintain the ring.

### 1. insert_after

Inserts a new node immediately after the specified current node.

```c
void insert_after(struct task_t *cur, struct task_t *new)
{
    new->next = cur->next;
    new->prev = cur;

    cur->next->prev = new;
    cur->next = new;
}

```

**Mental Model:** `cur ⇄ next` becomes `cur ⇄ new ⇄ next`.

### 2. insert_before

Inserts a new node immediately before the specified current node.

```c
void insert_before(struct task_t *cur, struct task_t *new)
{
    new->next = cur;
    new->prev = cur->prev;

    cur->prev->next = new;
    cur->prev = new;
}

```

**Mental Model:** `prev ⇄ cur` becomes `prev ⇄ new ⇄ cur`.

### 3. remove_task

Unlinks a node from the ring and resets its own pointers to form a 1-node ring.

```c
void remove_task(struct task_t *t)
{
    t->prev->next = t->next;
    t->next->prev = t->prev;

    t->next = t;
    t->prev = t;
}

```

**Mental Model:** `prev ⇄ t ⇄ next` becomes `prev ⇄ next`.

### 4. is_singleton

Detects if the ring contains only the specified node.

```c
int is_singleton(struct task_t *t)
{
    return (t->next == t);
}

```

---

## Traversal Operations (O(n))

Used for debugging or inspecting the task ring.

### 5. iterate_forward

```c
void iterate_forward(struct task_t *start)
{
    if (!start) return;
    struct task_t *cur = start;
    do {
        printf("%s\n", cur->name);
        cur = cur->next;
    } while (cur != start);
}

```

### 6. iterate_backward

```c
void iterate_backward(struct task_t *start)
{
    if (!start) return;
    struct task_t *cur = start;
    do {
        printf("%s\n", cur->name);
        cur = cur->prev;
    } while (cur != start);
}

```

---

## Runqueue-Level Control

### 7. add_to_runqueue

Adds a task to the global scheduler ring.

```c
void add_to_runqueue(struct task_t *t)
{
    if (!__tlist) {
        __tlist = t;
        return;
    }

    insert_after(__tlist, t);
}

```

### 8. remove_from_runqueue

Safely removes a task and updates the global list head if necessary.

```c
void remove_from_runqueue(struct task_t *t)
{
    if (t == __tlist) {
        if (is_singleton(t)) {
            __tlist = NULL;
        } else {
            __tlist = t->next;
        }
    }

    remove_task(t);
}

```

---

## The Ring Invariant

To ensure structural integrity, the following logic must always hold for every node `t`:

* `t->next->prev == t`
* `t->prev->next == t`
