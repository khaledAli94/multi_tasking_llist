# Runqueue Operations: Architectural View

## O(1) Operations - Constant Time
Fixed pointer manipulations independent of total task count.

### 1. insert_after -> O(1)
Splice `new` node directly after known node `cur`.
- Mechanism: `cur <-> next` becomes `cur <-> new <-> next`
- Cost: 4 pointer writes.

### 2. insert_before -> O(1)
Symmetric splicing before known node `cur`.
- Mechanism: `prev <-> cur` becomes `prev <-> new <-> cur`
- Cost: 4 pointer writes.

### 3. remove_task -> O(1)
Unlink node `t` and isolate it as a 1-node ring.
- Mechanism: `prev <-> t <-> next` becomes `prev <-> next`
- Traversal: None.

### 4. is_singleton -> O(1)
Identify if the ring contains only one node.
- Logic: `t->next == t`
- Cost: 1 comparison.

### 5. add_to_runqueue -> O(1)
- If empty: `__tlist = new`
- If populated: `insert_after(__tlist, new)`

### 6. remove_from_runqueue -> O(1)
- If removing current task: update head pointer to `next` (or NULL if last node).
- Execute `remove_task(t)`.

## O(n) Operations - Linear Time
Proportional to the number of tasks in the ring.

### 7. iterate_forward -> O(n)
Walk the ring via `next` pointers until returning to start.
- Path: `t -> t->next -> t->next->next -> ... -> t`

### 8. iterate_backward -> O(n)
Walk the ring via `prev` pointers until returning to start.

## Control Model Invariants
The system maintains integrity through the following pointer logic:
- `t->next->prev == t`
- `t->prev->next == t`

### Design Capabilities
- O(1) removal/insertion at any node.
- O(n) full list traversal.
- O(1) singleton detection.
- Dynamic runqueue scaling.
- Global scheduler pointer maintenance.
