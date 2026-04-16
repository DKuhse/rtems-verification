# Verification Header Set for RTEMS 6.2 EDF Scheduler

## Purpose

This directory contains **verification headers** — annotated or
simplified versions of RTEMS 6.2 headers used during Frama-C/WP
verification. There are two categories:

### Annotated headers (required)

Headers where ACSL contracts are added to inline functions that get
inlined into verified code. These MUST replace the originals because
WP needs contracts on the inlined functions. Currently:
- `priorityimpl.h` — ACSL contracts on priority aggregation functions

### Simplified headers (kept in reserve)

Headers where transitive includes are cut to reduce the number of
visible uncontracted inline functions. RTEMS 6.2 headers expose ~450
`static inline` functions after preprocessing (vs ~50 in 5.1).

**These are NOT currently deployed.** Testing showed that the 450
uncontracted functions don't interfere with verification — WP only
analyzes functions targeted by `-wp-fct`. The actual blocker was
flexible array members in `thread.h` (patched separately).

The simplified headers are kept here in case they're needed for
Phase 3 (Update_priority, Unblock) where `-inline-calls` expands the
analysis scope. They should only be deployed if testing shows they're
actually needed.

Currently in reserve:
- `headers/schedulerimpl.h` — cuts `threadimpl.h` and `smpimpl.h`
- `headers/threadimpl.h` — cuts 12 transitive includes (~200 functions)

## Assumptions

Each header introduces assumptions — things that are true in the real
header but not formally verified. Every assumption is documented
per-header below.

## Header inventory

Each header file has a corresponding section documenting:
- **Replaces**: which RTEMS 6.2 header, with path
- **Kept verbatim**: types, macros, and functions preserved unchanged
- **Removed**: what was dropped and why
- **Modified**: what was changed (code changes, not just added ACSL)
- **ACSL contracts added**: which functions got contracts
- **Assumptions**: what the simplification assumes

---

### priorityimpl.h

**Replaces**: `cpukit/include/rtems/score/priorityimpl.h` (766 lines → ~590 lines)

**Kept verbatim** (types and macros):
- `Priority_Group_order` enum
- `Priority_Add_handler`, `Priority_Change_handler`, `Priority_Remove_handler` typedefs

**Kept verbatim** (functions, with ACSL contracts added):
- `_Priority_Actions_initialize_empty`
- `_Priority_Actions_initialize_one`
- `_Priority_Actions_is_empty`
- `_Priority_Actions_move`
- `_Priority_Actions_add`
- `_Priority_Node_initialize`
- `_Priority_Node_set_priority`
- `_Priority_Node_set_inactive`
- `_Priority_Node_is_active`
- `_Priority_Initialize_empty`
- `_Priority_Initialize_one`
- `_Priority_Is_empty`
- `_Priority_Get_priority`
- `_Priority_Get_scheduler`
- `_Priority_Set_action_node`
- `_Priority_Set_action_type`
- `_Priority_Set_action`
- `_Priority_Less`
- `_Priority_Plain_insert`
- `_Priority_Plain_extract`
- `_Priority_Plain_changed`
- `_Priority_Change_nothing`
- `_Priority_Remove_nothing`
- `_Priority_Non_empty_insert`
- `_Priority_Insert`
- `_Priority_Extract`
- `_Priority_Extract_non_empty`
- `_Priority_Changed`
- `_Priority_Replace`

**Removed**:
- `_Priority_Actions_is_valid` — removed in RTEMS 6.2 (was uniprocessor no-op)
- `_Priority_Get_next_action` — SMP-only in 6.2, not needed for uniprocessor verification

**Modified** (code changes):
- `_Priority_Get_minimum_node`: body changed from
  `return (Priority_Node *) _RBTree_Minimum( &aggregation->Contributors );`
  to `return _Helper_RBTree_Minimum( &aggregation->Contributors );`
  **Reason**: `_RBTree_Minimum` traverses the red-black tree, which WP
  cannot reason about. The stub `_Helper_RBTree_Minimum` returns the
  ghost variable `g_min_priority_node` instead.

**ACSL added**:
- Ghost variables: `g_new_minimum`, `g_min_priority_node`,
  `g_scheduler_node_of_wait_priority_node`
- Stub declaration: `_Helper_RBTree_Minimum` with contract
- Contracts on all functions listed above (see file for details)
- `//@ calls` annotations on function pointer callbacks in
  `_Priority_Non_empty_insert`, `_Priority_Extract_non_empty`,
  `_Priority_Changed` — currently commented out because the callee
  `_Thread_Priority_action_change` is static in another translation unit

**Assumptions**:
- The red-black tree correctly maintains priority ordering (abstracted
  via `g_min_priority_node` ghost variable)
- `_Priority_Plain_insert` correctly reports whether a new minimum was
  inserted (abstracted via `g_new_minimum` ghost variable)
- `SCHEDULER_NODE_OF_WAIT_PRIORITY_NODE` macro correctly computes the
  containing `Scheduler_Node` pointer (abstracted via
  `g_scheduler_node_of_wait_priority_node` ghost variable)
- `_Priority_Get_next_action` is not called in the uniprocessor
  configuration (SMP-only in 6.2)

---

---

## Include chain analysis

The source file `scheduleredfreleasejob.c` includes `scheduleredfimpl.h`,
which starts the chain:

```
scheduleredfimpl.h
└── scheduleruniimpl.h
    └── schedulerimpl.h          (25 inline functions)
        ├── smpimpl.h
        │   └── processormaskimpl.h  (22)
        ├── threadimpl.h          (71 inline functions — biggest contributor)
        │   ├── objectimpl.h      (23)
        │   ├── schedulernodeimpl.h
        │   ├── statesimpl.h      (9)
        │   ├── threadqimpl.h     (29)
        │   ├── todimpl.h         (10)
        │   │   └── timecounterimpl.h
        │   ├── watchdogimpl.h    (26)
        │   │   └── rbtreeimpl.h
        │   └── config.h → ...
        └── priorityimpl.h        (30 — already annotated)
```

Total: ~450 `static inline` functions visible after preprocessing.
Of these, ~10-15 are needed for EDF scheduler verification.

### Headers that need simplified versions

| Header | Functions | Needed | Strategy |
|--------|-----------|--------|----------|
| `priorityimpl.h` | 30 | most | **Done** — annotated version created |
| `threadimpl.h` | 71 | ~5 | Simplified version needed |
| `schedulerimpl.h` | 25 | ~2 | Simplified version needed |
| `threadqimpl.h` | 29 | ~1 | Cut via `threadimpl.h` simplification |
| `chainimpl.h` | 41 | 0 | Cut via `threadimpl.h` simplification |
| `atomic.h` | 30 | 0 | Cut via `threadimpl.h` simplification |
| `watchdogimpl.h` | 26 | 0 | Cut via `threadimpl.h` simplification |
| `objectimpl.h` | 23 | 0 | Cut via `threadimpl.h` simplification |
| `processormaskimpl.h` | 22 | 0 | Cut via `schedulerimpl.h` simplification |
| `rbtree.h` | 19 | ~2 | Keep (contracts on `_RBTree_Is_node_off_tree`, `_RBTree_Set_off_tree`) |

The key insight: simplifying `threadimpl.h` and `schedulerimpl.h` would
cut off most of the transitive includes, reducing the visible inline
function count from ~450 to ~60-80.

---

*Additional headers will be documented here as they are created.*
