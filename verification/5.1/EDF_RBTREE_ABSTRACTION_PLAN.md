# EDF Scheduler Verification with Abstract RBTree Model (RTEMS 5.1 port)

This plan is the 5.1 back-port of the active 6.2 abstraction plan
(`../6.2/EDF_RBTREE_ABSTRACTION_PLAN.md`). The modeling approach, contract
strategy, and acceptance criteria are unchanged. This file records only the
5.1-specific differences. See the 6.2 plan for the full rationale.

## Summary

Verify the RTEMS 5.1 EDF scheduler against an abstract model of the RTEMS
red-black tree. RBTree implementation is out of scope; EDF proofs rely on
explicit contracts for abstract tree behavior.

## Concrete RBTree Locations (RTEMS 5.1)

- `rtems/src/rtems-5.1-pristine/cpukit/include/rtems/score/rbtree.h`
- `rtems/src/rtems-5.1-pristine/cpukit/include/rtems/score/rbtreeimpl.h`
- `rtems/src/rtems-5.1-pristine/cpukit/include/rtems/score/bsd-tree.h`
- `rtems/src/rtems-5.1-pristine/cpukit/score/src/rbtree*.c`

EDF specifically:

- `Scheduler_EDF_Context::Ready` — EDF ready RBTree
- `Scheduler_EDF_Node::Node` — embedded RBTree node

Thread priority aggregation:

- `Priority_Aggregation::Contributors` — active priority contributors

These layouts match 6.2 exactly.

## 5.1-Specific Differences from the 6.2 Plan

### No uniprocessor helper layer

6.2 places contract boundaries on
`_Scheduler_uniprocessor_Update_heir`/`_..._Update_heir_if_preemptible`/
`_..._Update_heir_if_necessary`/`_..._Block`/`_..._Unblock`/
`_..._Schedule`/`_..._Yield`. None of these exist in 5.1.

The 5.1 port instead places contract boundaries on:

- `_Scheduler_EDF_Schedule_body()` (covers 6.2's
  `_Scheduler_EDF_Get_highest_ready` + `_Scheduler_uniprocessor_Update_heir`)
- `_Scheduler_Generic_block()` (covers 6.2's `_Scheduler_uniprocessor_Block`)
- the EDF entry points where 5.1 inlines the heir update — notably
  `_Scheduler_EDF_Unblock` (covers 6.2's `_Scheduler_uniprocessor_Unblock`)

### No `_Scheduler_EDF_Get_highest_ready` function

5.1 has no equivalent. Any 6.2 contract referring to
`_Scheduler_EDF_Get_highest_ready` must be re-expressed against
`_Scheduler_EDF_Schedule_body`.

### Priority group ordering

5.1 uses `bool prepend_it` rather than a `Priority_Group_order` enum.
Contracts on `_Scheduler_Node_set_priority` and the priority-action paths
in `threadchangepriority.c` must use the 5.1 calling convention.

### `_Thread_Get_CPU_time_used` signature

5.1 returns `void` via an out-pointer and the caller checks an additional
condition. The 6.2 contracts return a value. The rate-monotonic contracts
must be re-derived against the 5.1 signature.

## Verification Order

Same order as 6.2:

1. Abstract RBTree model + contracts on `_Scheduler_EDF_Enqueue`,
   `_Scheduler_EDF_Extract`, `_Scheduler_EDF_Get_highest_ready` (or rather
   the 5.1 `_Scheduler_EDF_Schedule_body`).  [PARTIALLY DONE]
2. EDF helper layer in `scheduleredfimpl.h`.  [PARTIALLY DONE]
3. Simple EDF functions: `Map_priority`, `Unmap_priority`, `Initialize`,
   `Node_initialize`.  [INITIALIZE/NODE_INITIALIZE DONE]
4. Heir-update contract boundaries: `_Scheduler_EDF_Schedule_body` and
   `_Scheduler_Generic_block`.  [SCHEDULE_BODY DONE]
5. EDF ready-queue entry points: `_Scheduler_EDF_Unblock`,
   `_Scheduler_EDF_Update_priority`, `_Scheduler_EDF_Schedule`,
   `_Scheduler_EDF_Block`, `_Scheduler_EDF_Yield`.  [PENDING]
6. EDF release/cancel: `_Scheduler_EDF_Release_job`,
   `_Scheduler_EDF_Cancel_job`.  [PENDING]

## Acceptance Criteria

Same as 6.2 (see `../6.2/EDF_RBTREE_ABSTRACTION_PLAN.md`).
