# Notes on Porting verification/6.2/ to verification/5.1/

The active RTEMS 6.2 verification effort (`../6.2/`) is the reference for this
port. This document captures the differences that matter when adapting 6.2
ACSL contracts to the RTEMS 5.1 code shape.

## What transfers 1:1

- `models/edf_ready_set.h`, `models/edf_property.h`, `models/priority_aggregation.h`,
  `models/thread_priority_updates.h` — only include-guard names change.
- `Scheduler_EDF_Node`, `Scheduler_EDF_Context`, `Priority_Aggregation`,
  `Priority_Node`, `Thread_Control.Scheduler.nodes` layouts.
- All EDF entry-point signatures (`_Scheduler_EDF_Initialize`,
  `_Scheduler_EDF_Block`, `_Scheduler_EDF_Unblock`, `_Scheduler_EDF_Schedule`,
  `_Scheduler_EDF_Yield`, `_Scheduler_EDF_Update_priority`,
  `_Scheduler_EDF_Release_job`, `_Scheduler_EDF_Cancel_job`).
- `_Scheduler_EDF_Get_context`, `_Scheduler_EDF_Node_downcast`,
  `_Scheduler_EDF_Enqueue`, `_Scheduler_EDF_Extract`,
  `_Scheduler_EDF_Extract_body` contracts (modulo `RTEMS_INLINE_ROUTINE` vs
  `static inline` cosmetic).
- `_Scheduler_EDF_Initialize`, `_Scheduler_EDF_Node_initialize` source-side
  contracts (function bodies are line-for-line identical).
- Frama-C compatibility patches: comment out `<limits.h>` in `scheduleredf.h`,
  forward-declare `tstosbt`/`sbttots`/`sbttotv` under `__FRAMAC__`, single-element
  `_Per_CPU_Information[1U]` array + dispatch-necessary volatile binding.

## What requires adaptation

### Pseudo-ISR force-dispatch in `_Scheduler_EDF_Unblock` (carved out)

5.1's `_Scheduler_EDF_Unblock` has a pseudo-ISR escape hatch that 6.2 does
not: `_Scheduler_Update_heir(the_thread, priority == (SCHEDULER_EDF_PRIO_MSB |
PRIORITY_PSEUDO_ISR))`. When a non-preemptible heir is force-dispatched to a
pseudo-ISR-priority `the_thread`, the new heir is not generally the
earliest-ready node (deadline threads with smaller priority values may be
present), so P3.a (`is_preemptible ==> heir owns earliest-ready`) cannot
hold if the new heir is preemptible.

The 5.1 active contract carves this path out via the precondition

```
requires SCHEDULER_PRIORITY_PURIFY( node->Priority.value ) !=
  ( SCHEDULER_EDF_PRIO_MSB | PRIORITY_PSEUDO_ISR );
```

so the verified region matches 6.2's behavior exactly (no force-dispatch).
RTEMS callers honor this naturally: pseudo-ISR-priority threads are
limited to the MPCI receive server (`mpci.c:135`) and the timer server
(`timerserver.c:163`), both configured non-preemptible by convention
(`timerserver.c:187` uses `RTEMS_NO_PREEMPT`; `mpci.c` zero-inits its
config so `is_preemptible = false`). Ratemon period tasks have
deadline-derived priorities and are never pseudo-ISR.

### `_Scheduler_uniprocessor_*` helpers do not exist in 5.1

6.2 introduces a `<rtems/score/scheduleruniimpl.h>` family
(`_Scheduler_uniprocessor_Update_heir`, `_..._Update_heir_if_necessary`,
`_..._Update_heir_if_preemptible`, `_..._Block`, `_..._Unblock`,
`_..._Schedule`, `_..._Yield`) used as a contract boundary for the heir
update.

In 5.1:

- `_Scheduler_EDF_Unblock()` inlines the heir update directly.
- `_Scheduler_EDF_Block()` and `_Scheduler_EDF_Yield()` use the
  scheduler-agnostic `_Scheduler_Generic_block(extract, schedule)` from
  `schedulerimpl.h`.
- `_Scheduler_EDF_Schedule()` ultimately calls
  `_Scheduler_EDF_Schedule_body(scheduler, thread, force_dispatch)`, which
  finds the EDF-earliest ready node and calls `_Scheduler_Update_heir()`
  with the chosen owner.

The 5.1 active port keeps the contract boundary at:

- `_Scheduler_EDF_Schedule_body()` (replaces 6.2's combination of
  `_Scheduler_EDF_Get_highest_ready` + `_Scheduler_uniprocessor_Update_heir`).
  The contract states the new heir is an EDF-earliest ready owner and the
  dispatch ghost mirror becomes true.
- `_Scheduler_Generic_block()` (verification-only contract, modeled on 6.2's
  `_Scheduler_uniprocessor_Block`).
- The EDF entry points themselves where 5.1 inlines the heir-update logic
  (notably `_Scheduler_EDF_Unblock`).

### `_Scheduler_EDF_Get_highest_ready()` is new in 6.2

5.1 does not have this function. The 6.2 contracts that reference
`_Scheduler_EDF_Get_highest_ready` have to be rewritten against
`_Scheduler_EDF_Schedule_body` (which performs both lookup and heir update).

### `Priority_Group_order` enum vs `bool prepend_it`

5.1 uses `bool prepend_it` in `_Scheduler_Node_set_priority()` while 6.2
uses a `Priority_Group_order` enum (`PRIORITY_GROUP_FIRST` = 0 corresponds
to `prepend_it = true`; `PRIORITY_GROUP_LAST` = 1 corresponds to
`prepend_it = false` and sets the append flag).

Contracts on `_Scheduler_Node_set_priority()` and the
`threadchangepriority.c` priority-action paths must use the 5.1 calling
convention. The 6.2 `Priority_Group_order` enum is not available in 5.1.

### `_Thread_Get_CPU_time_used()` has different signature

5.1: `void _Thread_Get_CPU_time_used( Thread_Control *, Timestamp_Control * )`
6.2: `Timestamp_Control _Thread_Get_CPU_time_used( Thread_Control * )`

The `ratemonperiod.c` overlay must therefore re-derive the rate-monotonic
contract against the 5.1 out-pointer signature. The 5.1 version also has a
defensive CPU-reset check (returns `false`) that 6.2 removed; the bool
return value must be preserved by the 5.1 contract.

### Entry-point table macros renamed

`SCHEDULER_OPERATION_DEFAULT_ASK_FOR_HELP` (5.1) →
`SCHEDULER_DEFAULT_SMP_OPERATIONS` (6.2).

`SCHEDULER_OPERATION_DEFAULT_GET_SET_AFFINITY` (5.1) →
`SCHEDULER_DEFAULT_SET_AFFINITY_OPERATION` (6.2).

5.1 also has a `_Scheduler_default_Tick` entry slot which 6.2 removed.

These do not affect contracts directly but are visible in `scheduleredf.h`.

### `SCHEDULER_PRIORITY_APPEND_FLAG` vs `PRIORITY_GROUP_LAST`

5.1 defines `SCHEDULER_PRIORITY_APPEND_FLAG 1` directly. 6.2 references
`PRIORITY_GROUP_LAST` (= 1) from `priorityimpl.h`. Value-equivalent.

### `RTEMS_INLINE_ROUTINE` vs `static inline`

Cosmetic only. The 5.1 overlay keeps `RTEMS_INLINE_ROUTINE`.
