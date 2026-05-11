# Porting Verification from RTEMS 5.1 to 6.2

## Overview

Three categories of change:
1. **Mechanical** — `bool prepend_it` → `Priority_Group_order` enum (most functions)
2. **Architectural** — uniprocessor scheduler abstraction layer replaces inline logic
3. **Unchanged** — several EDF-specific functions are identical

## Function-by-function mapping

### Unchanged (contracts can be reused directly)

| Function | File | Notes |
|----------|------|-------|
| `_Scheduler_EDF_Cancel_job` | scheduleredfreleasejob.c | Identical |
| `_Scheduler_EDF_Map_priority` | scheduleredfreleasejob.c | Identical |
| `_Scheduler_EDF_Unmap_priority` | scheduleredfreleasejob.c | Identical |

### Mechanical changes (`bool` → `Priority_Group_order` enum)

The new enum:
```c
typedef enum {
  PRIORITY_GROUP_FIRST = 0,  // was true  (prepend)
  PRIORITY_GROUP_LAST = 1    // was false (append)
} Priority_Group_order;
```

| Function | File | What changed |
|----------|------|-------------|
| `_Thread_Set_scheduler_node_priority` | threadchangepriority.c | Parameter type only |
| `_Thread_Priority_action_change` | threadchangepriority.c | Parameter type; added `(void) arg;` |
| `_Thread_Priority_changed` | threadchangepriority.c | Parameter type only |
| `_Thread_Priority_add` | threadchangepriority.c | `false` → `PRIORITY_GROUP_LAST` |
| `_Thread_Priority_remove` | threadchangepriority.c | `true` → `PRIORITY_GROUP_FIRST` |
| `_Thread_Priority_do_perform_actions` | threadchangepriority.c | Parameter type; SMP loop termination improved |
| `_Priority_Non_empty_insert` | priorityimpl.h | `false` → `PRIORITY_GROUP_LAST` in callback |
| `_Priority_Extract_non_empty` | priorityimpl.h | `true` → `PRIORITY_GROUP_FIRST` in callback |
| `_Priority_Changed` | priorityimpl.h | Parameter type; added `_Assert(min != NULL)` |
| `_Scheduler_EDF_Release_job` | scheduleredfreleasejob.c | `false` → `PRIORITY_GROUP_LAST` in `_Thread_Priority_changed` call |
| `_Scheduler_Node_set_priority` | schedulernodeimpl.h | Parameter type; uses `(Priority_Control) group_order` directly |

For these, the ACSL contracts need the `prepend_it` behaviors renamed:
```
behavior prepend:     →  behavior group_first:
  assumes prepend_it;       assumes priority_group_order == PRIORITY_GROUP_FIRST;

behavior no_prepend:  →  behavior group_last:
  assumes !prepend_it;      assumes priority_group_order == PRIORITY_GROUP_LAST;
```

The `| 0` / `| 1` logic in ensures clauses stays the same since
`PRIORITY_GROUP_FIRST == 0` and `PRIORITY_GROUP_LAST == 1`.

### Structural changes (need rework)

#### `_Thread_Priority_apply`
Parameter type change plus SMP path acquire/release added around the
perform-actions call. For uniprocessor verification, the SMP guards are
compiled out, so the non-SMP path is similar to 5.1. Contracts need
updating but the core logic is the same.

#### `_Scheduler_EDF_Update_priority`
The call `_Scheduler_EDF_Schedule_body(scheduler, the_thread, false)` is
replaced with:
```c
_Scheduler_uniprocessor_Schedule(scheduler, _Scheduler_EDF_Get_highest_ready);
```
The function itself is simpler — the scheduling decision is now delegated
to the uniprocessor layer. The contract needs to reference the new call
but the EDF-specific logic (extract, re-enqueue, check priority) is the
same.

#### `_Scheduler_EDF_Unblock`
The manual heir comparison + `_Scheduler_Update_heir` call is replaced
with:
```c
_Scheduler_uniprocessor_Unblock(scheduler, the_thread, priority);
```
The priority comparison and preemption check are now inside
`_Scheduler_uniprocessor_Unblock`. Contract needs complete rewrite to
match the new call structure.

### Removed (replaced by new uniprocessor layer)

| 5.1 Function | 6.2 Replacement | File |
|-------------|----------------|------|
| `_Scheduler_Update_heir` | `_Scheduler_uniprocessor_Update_heir` | scheduleruniimpl.h |
| `_Scheduler_EDF_Schedule_body` | `_Scheduler_uniprocessor_Schedule` + `_Scheduler_EDF_Get_highest_ready` | scheduleruniimpl.h + scheduleredfimpl.h |

### New uniprocessor layer (scheduleruniimpl.h) — should get standalone contracts

These functions replace `_Scheduler_Update_heir` and
`_Scheduler_EDF_Schedule_body` from 5.1. In 5.1, their predecessors were
always inlined — WP analyzed the code body, never the contract. But the
6.2 architecture argues for a different approach: **verify these with
standalone contracts and use the contracts as abstraction boundaries.**

Reasons:

1. **Modularity.** The paper's thesis is modular verification. In 5.1
   the uniprocessor logic was mixed into EDF code, so inlining was the
   only option. In 6.2, RTEMS has done the modular separation — the
   common logic is shared by all uniprocessor schedulers (EDF,
   fixed-priority, etc.). Verify once, reuse for all.

2. **Simpler outer contracts.** Instead of `_Scheduler_EDF_Update_priority`
   reasoning through 3 levels of inlined code, it calls
   `_Scheduler_uniprocessor_Schedule` against a contract: "the
   highest-priority ready thread becomes heir if the current heir is
   preemptible." The EDF contract only proves correct extract, re-enqueue,
   and callback selection.

3. **Volatile isolation.** The `_Thread_Dispatch_necessary` limitation
   lives inside `_Scheduler_uniprocessor_Update_heir`. Its contract says
   "heir is updated" but leaves `dispatch_necessary` unproved. Outer EDF
   contracts never touch the volatile — they rely on the heir-update
   guarantee.

4. **No more deep `-inline-calls` chains.** Each layer is verified against
   its contract. This is how Frama-C/WP is designed to work — function
   contracts as abstraction boundaries — rather than fighting it with
   deep inlining.

The 5.1 verification had to work around the code structure (inlining,
ghost variables for casts, `\separated` for Per_CPU). The 6.2
architecture is naturally modular — the code structure now matches the
verification approach.

| Function | Purpose |
|----------|---------|
| `_Scheduler_uniprocessor_Update_heir` | Sets new heir, updates CPU time, sets dispatch flag |
| `_Scheduler_uniprocessor_Update_heir_if_necessary` | Calls above only if heir differs |
| `_Scheduler_uniprocessor_Update_heir_if_preemptible` | Calls above only if heir differs AND is preemptible |
| `_Scheduler_uniprocessor_Block` | Extract + schedule highest ready if blocking heir |
| `_Scheduler_uniprocessor_Unblock` | Compare priority + conditionally update heir |
| `_Scheduler_uniprocessor_Schedule` | Get highest ready via callback + update heir |
| `_Scheduler_uniprocessor_Yield` | Get highest ready + unconditionally update if different |

With standalone contracts, no `-inline-calls` for these functions —
WP uses the contracts directly. The EDF functions call into the
uniprocessor layer via contract composition instead of inlining.

`_Scheduler_uniprocessor_Update_heir` is the direct successor of
`_Scheduler_Update_heir` from 5.1. It contains the same
`_Thread_Dispatch_necessary = true` assignment (still `volatile bool`,
same limitation applies — but now isolated to this one function's
contract rather than leaking into all outer proofs).

### New in 6.2: `_Scheduler_EDF_Get_highest_ready`

Replaces the inline tree-minimum lookup that was inside
`_Scheduler_EDF_Schedule_body`:
```c
static inline Thread_Control *_Scheduler_EDF_Get_highest_ready(
  const Scheduler_Control *scheduler)
{
  Scheduler_EDF_Context *context = _Scheduler_EDF_Get_context(scheduler);
  RBTree_Node *first = _RBTree_Minimum(&context->Ready);
  Scheduler_EDF_Node *node = RTEMS_CONTAINER_OF(first, Scheduler_EDF_Node, Node);
  return node->Base.owner;
}
```
This needs a contract and likely a ghost variable for the tree minimum
(same approach as `_Helper_RBTree_EDF_Minimum` in 5.1 stubs).

## Frama-C compatibility patches for 6.2

| 5.1 Patch | Still needed in 6.2? |
|-----------|---------------------|
| Comment out `#include <limits.h>` in scheduleredf.h | Needs investigation (was it the same issue?) |
| Comment out line 883 in thread.h | **No** — line removed upstream |
| `_Per_CPU_Information[]` → `[1U]` in percpu.h | **Yes** — still flexible array |

## Build system

RTEMS 6.2 uses **waf** instead of autotools. The BSP build step in
`setup.sh` needs a different command sequence. The RSB for 6.2 builds a
newer GCC (check RSB 6.2 for exact version).

## Suggested porting order

1. Get RTEMS 6.2 building and preprocessing with Frama-C (toolchain +
   BSP + patches)
2. Port the unchanged functions first (Cancel_job, Map/Unmap_priority)
   — validate the infrastructure works
3. Port the mechanical bool→enum changes (bulk of the work, but
   straightforward)
4. Write fresh contracts for `scheduleruniimpl.h` functions
5. Port the structural changes (Update_priority, Unblock) using the new
   uniprocessor layer contracts
6. Verify the full suite
