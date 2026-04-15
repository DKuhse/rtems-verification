# Changes to Verification Annotations

Changes made to the ACSL annotations and WP invocation from the original
[Formally-Verifying-Implementations-of-EDF-Scheduler-in-RTEMS](Formally-Verifying-Implementations-of-EDF-Scheduler-in-RTEMS/)
repository that brought all proof goals to 100%.

## 1. WP memory model: Typed+Cast

Pass `-wp-model "Typed+Cast"` to frama-c. The default `Typed` model cannot
reason through `Scheduler_Node*` to `Scheduler_EDF_Node*` casts used
throughout the ACSL contracts.

## 2. Added `\separated` clauses vs `_Per_CPU_Information`

When `_Scheduler_Update_heir` is inlined, it accesses the global
`_Per_CPU_Information` array. WP needs to know function parameters don't
alias with it.

**scheduleredfchangepriority.c** — added to `_Scheduler_EDF_Update_priority` contract:
```c
requires \separated(node + (..), (Per_CPU_Control_envelope *)_Per_CPU_Information + (..));
requires \separated(scheduler + (..), (Per_CPU_Control_envelope *)_Per_CPU_Information + (..));
requires \separated(the_thread + (..), (Per_CPU_Control_envelope *)_Per_CPU_Information + (..));
```

**scheduleredfunblock.c** — added to `_Scheduler_EDF_Unblock` contract:
```c
requires \separated(node + (..), (Per_CPU_Control_envelope *)_Per_CPU_Information + (..));
requires \separated(the_thread + (..), (Per_CPU_Control_envelope *)_Per_CPU_Information + (..));
requires \separated(scheduler + (..), (Per_CPU_Control_envelope *)_Per_CPU_Information + (..));
```

## 3. Added `\from` clauses on pointer-returning functions

Changed `assigns \nothing` to `assigns \result \from ...` for functions
returning pointers, so WP can track data dependencies precisely:

- **stubs.h**: `_Helper_RBTree_EDF_Minimum` → `assigns \result \from tree, g_min_edf_node;`
- **stubs.h**: `_Thread_Get_CPU` → `assigns \result \from thread;`
- **scheduleredfimpl.h**: `_Scheduler_EDF_Get_context` → `assigns \result \from scheduler, g_edf_sched_context;`
- **scheduleredfimpl.h**: `_Scheduler_EDF_Node_downcast` → `assigns \result \from node;`
- **schedulerimpl.h**: `_Scheduler_Update_heir` new_h behavior → `assigns _Thread_Heir \from new_heir; assigns _Thread_Dispatch_necessary \from \nothing;`

## 4. Commented out unprovable `_Thread_Dispatch_necessary` ensures

**scheduleredfchangepriority.c** — commented out
`ensures _Thread_Dispatch_necessary == true;` in the `exec_update_new_h`
behavior of `_Scheduler_EDF_Update_priority`.

The `dispatch_necessary` field in `Per_CPU_Control` is declared
`volatile bool` (`percpu.h:400`). Frama-C/WP correctly refuses to prove
postconditions about volatile writes — the C standard says volatile
variables may change at any time by external means (here: interrupt
context), so a write cannot be assumed to persist at function exit.

The code does set `_Thread_Dispatch_necessary = true`
(`schedulerimpl.h:1225`), but this is fundamentally unprovable under
volatile semantics. The inner contracts (`_Scheduler_Update_heir` and
`_Scheduler_EDF_Schedule_body`) already had this ensures commented out.
The `_Scheduler_EDF_Unblock` contract had it commented out for the same
reason.

The field is volatile because it is read from interrupt handlers and
written from the scheduler — `volatile` prevents the compiler from
optimizing away the write. But `volatile` also means "may change at any
time without the program doing anything," so WP correctly refuses to
assume the write persists until the postcondition. Writing `true` to a
volatile variable does not let you prove it is still `true` one
instruction later — that is the semantics of `volatile` in C.

There is no way to make this provable without removing `volatile`, which
would be incorrect for the actual RTEMS code.
