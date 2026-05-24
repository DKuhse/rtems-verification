# Active RTEMS 5.1 Verification Header Inventory

This file tracks active RTEMS 5.1 verification overlays and stubs.

The legacy in-tree 5.1 verification (which patches `rtems/src/rtems-5.1/`
directly via `setup.sh`) is still functional and lives at
`Formally-Verifying-Implementations-of-EDF-Scheduler-in-RTEMS/`. The new
overlay-style verification documented here does NOT patch the pristine 5.1
source tree; instead the overlay is preferred via `-I` ordering when running
Frama-C through `scripts/5.1-active/`.

## Current State

The active 5.1 tree contains a starting overlay derived from the active 6.2
verification effort (`../6.2/`). Most files are currently pristine 5.1 imports
with the contracts ported file-by-file as they reach a clean state. Already in
place:

- Frama-C compatibility patches in `scheduleredf.h`, `thread.h`, `percpu.h`,
  and `scheduler.h` (the `char empty;` fix for the 5.1 non-SMP empty
  `Scheduler_Context`)
- Volatile dispatch-necessary ghost mirror binding in `percpu.h`
- Abstract models (`models/`) — same content as 6.2 modulo include-guard
- `_Scheduler_Get_context()` contract in `schedulerimpl.h` (required to
  unhang the cast-through-empty-struct path)
- `_RBTree_Initialize_empty()` contract in `rbtree.h`
- `scheduleredfimpl.h` contracts for `_Scheduler_EDF_Get_context`,
  `_Scheduler_EDF_Node_downcast`, `_Scheduler_EDF_Enqueue`,
  `_Scheduler_EDF_Extract`, `_Scheduler_EDF_Extract_body`
- Source contracts for `_Scheduler_EDF_Initialize`,
  `_Scheduler_EDF_Node_initialize`

**Verified slices (2026-05-20)**:

| Script | WP result | Notes |
|---|---|---|
| `verify-scheduler-update-heir.sh` | 28 / 28 | dedicated harness for `_Scheduler_Update_heir`; mirrors 6.2's `verify-scheduleruni-unblock.sh` two-tier setup |
| `verify-edf-initialize.sh` | 31 / 31 | parity with 6.2 |
| `verify-edf-node-initialize.sh` | 30 / 30 | parity with 6.2 |
| `verify-edf-map-unmap.sh` | 10 / 10 | new, dedicated to the Map/Unmap helpers |
| `verify-edf-schedule.sh` (function) | 68 / 68 | EDF entry point + `_Scheduler_EDF_Schedule_body` against its body. The `_RBTree_Minimum` contract (in `models/edf_property.h`) bridges the abstract ready set to the body's `RTEMS_CONTAINER_OF`. |
| `verify-edf-schedule.sh` (model lemma) | 11 / 11 | parity with 6.2 |
| `verify-edf-yield.sh` (function) | 54 / 54 | EDF Yield entry point. Mirrors 6.2's contract using `edf_scheduler_decision{Post}`, but routes through `_Scheduler_EDF_Schedule_body(scheduler, the_thread, true)` instead of 6.2's `_Scheduler_uniprocessor_Yield`. |
| `verify-edf-yield.sh` (model lemma) | 11 / 11 | parity with 6.2 |

**Verification architecture** (two-tier, mirrors 6.2):

1. Heir-update primitives are verified against their bodies in dedicated
   harnesses (5.1: `verify-scheduler-update-heir.sh` →
   `harnesses/scheduler-update-heir-harness.c`).
2. EDF entry-point scripts verify the entry point AND its EDF inline
   helpers (e.g. `_Scheduler_EDF_Schedule_body`) against their bodies,
   using verified primitive contracts from (1). The only trusted boundary
   is `_RBTree_Minimum` itself, contracted in
   `models/edf_property.h` against the abstract ready-set model. 6.2
   puts the same trust boundary at `_Scheduler_EDF_Get_highest_ready`,
   which exists as a real function in 6.2 but not in 5.1.

**Required helper contracts beyond 6.2**:

- `_Thread_Get_CPU()` in `threadimpl.h` — must state the non-SMP result is
  `&_Per_CPU_Information[0].per_cpu`. Without this, `_Scheduler_Update_heir`'s
  cpu_time_used frame parts time out.
- `_Scheduler_Get_context()` in `schedulerimpl.h` — required so the
  `Scheduler_Context *` ↔ `Scheduler_EDF_Context *` cast bridges cleanly.

Both contracts are forced by 5.1's helper layout (combined `_Scheduler_Update_heir`
instead of 6.2's split uniprocessor helpers; pristine `Scheduler_Context`
with empty body in non-SMP).

Pending (copied as pristine, contracts not yet ported):

- `schedulerimpl.h` — `_Scheduler_Get_context()` has its contract;
  still needs `_Scheduler_Generic_block` contract analogous to 6.2's
  `_Scheduler_uniprocessor_Block` contract
- `priorityimpl.h`, `schedulernodeimpl.h` — needs `_Priority_Get_priority`,
  `_Scheduler_Node_get_priority`, `_Scheduler_Node_set_priority` contracts.
  `set_priority` uses 5.1's `bool prepend_it` calling convention.
- `threadimpl.h`, `threadqimpl.h` — needs `_Thread_Get_priority`,
  `_Thread_Scheduler_get_home_node`, `_Thread_Priority_add/remove/changed`,
  `_Thread_queue_Context_add_priority_update`
- `scheduleredfblock.c` — needs contract on entry point + the
  `_Scheduler_Generic_block(_Scheduler_EDF_Extract_body, _Scheduler_EDF_Schedule_body)`
  call site
- `scheduleredfschedule.c` — needs contract on entry point referring back to
  `_Scheduler_EDF_Schedule_body`
- `scheduleredfunblock.c` — needs entry-point contract. 5.1 inlines the heir
  update, so the contract is shaped like 6.2's
  `_Scheduler_uniprocessor_Unblock` contract but applied to the EDF entry
  point directly.
- `scheduleredfyield.c` — DONE. Entry-point contract mirrors 6.2 using
  `edf_scheduler_decision{Post}`. Routes through `_Scheduler_EDF_Schedule_body`
  with `force_dispatch=true` (5.1 has no `_Scheduler_uniprocessor_*` family).
- `scheduleredfchangepriority.c` — needs update-priority contract
- `scheduleredfreleasejob.c` — needs release/cancel contracts (depend on
  priority aggregation)
- `threadchangepriority.c` — needs `_Thread_Priority_apply` and
  do-nothing-callback scaffold contracts (5.1 calling convention)
- `ratemonperiod.c`, `ratemoncancel.c` — needs rate-monotonic contracts.
  Note 5.1 `_Thread_Get_CPU_time_used()` uses an out-pointer and returns
  bool; the 6.2 contract must be re-derived.

## Intended Categories

- `overlay/cpukit/include/rtems/score/` — annotated headers that
  intentionally shadow pristine RTEMS 5.1 headers
- `overlay/cpukit/score/src/` — annotated source files passed directly to
  Frama-C
- `overlay/cpukit/rtems/src/` — annotated rate-monotonic source files
- `harnesses/` — small translation units used to verify header-only inline
  helpers (mirrors 6.2 layout; may not be needed since 5.1 has no
  `scheduleruniimpl.h`)
- `models/` — verification-only model contracts (1:1 with 6.2)

## Rule

Every active overlay or stub added here must document:

- which pristine RTEMS file or function it replaces or abstracts
- what behavior is preserved verbatim
- what behavior is changed for verification
- what assumptions are trusted rather than proved

## Baseline Imports

### scheduler.h

**Source**: `cpukit/include/rtems/score/scheduler.h` (5.1)

**Status**: active compatibility patch.

**Modified**:

- Under `__FRAMAC__` (non-SMP), added a `char empty;` filler to
  `Scheduler_Context`. In 5.1 the non-SMP body of this struct is just
  `ISR_LOCK_MEMBER( Lock )`, which expands to nothing, leaving an empty
  struct.  Empty structs trip WP's `Typed+Cast` model when reasoning about
  `(Scheduler_EDF_Context *) scheduler->context` casts — goal generation
  hangs forever, even for trivial contracts.  Mirrors the 6.2 layout
  trick under Frama-C.

### schedulerimpl.h

**Source**: `cpukit/include/rtems/score/schedulerimpl.h` (5.1)

**Status**: active compatibility patch (partial).

**Modified**:

- Added a contract for `_Scheduler_Get_context()`. Without this contract
  WP inlines its body through the `Scheduler_Context *` ↔
  `Scheduler_EDF_Context *` cast, and the assigns/ensures clauses of the
  EDF wrapper time out.

**Pending**: contracts for `_Scheduler_Generic_block`, `_Scheduler_Update_heir`,
`_Scheduler_Release_job`, `_Scheduler_Cancel_job`, and the SMP-only helpers
once their EDF entry-point callers are ported. The active 6.2 overlay
`schedulerimpl.h` is the reference.

### scheduleredf.h

**Source**: `cpukit/include/rtems/score/scheduleredf.h` (5.1)

**Status**: active compatibility patch.

**Modified**:

- Commented out `#include <limits.h>` for Frama-C preprocessing compatibility.
- Includes `edf_ready_set.h` and `edf_property.h` when `__FRAMAC__` is defined
  so contracts on EDF declarations can use the verification models after
  `Scheduler_EDF_Context` and `Scheduler_EDF_Node` are declared.

### scheduleredfimpl.h

**Source**: `cpukit/include/rtems/score/scheduleredfimpl.h` (5.1)

**Status**: active compatibility patch.

**Modified**:

- Added ACSL contracts for `_Scheduler_EDF_Get_context()`,
  `_Scheduler_EDF_Node_downcast()`, `_Scheduler_EDF_Enqueue()`,
  `_Scheduler_EDF_Extract()`, `_Scheduler_EDF_Extract_body()` — these are
  the same shape as the active 6.2 contracts because 5.1's helper bodies and
  signatures match.
- Added a new ACSL contract for `_Scheduler_EDF_Schedule_body()`. This is
  the 5.1 analogue of 6.2's `_Scheduler_EDF_Get_highest_ready` +
  `_Scheduler_uniprocessor_Update_heir` composition: the function finds the
  EDF-earliest ready node and unconditionally writes `_Thread_Heir` and the
  dispatch ghost mirror. There is no equivalent helper in 6.2 because 6.2
  splits these effects across two functions.

### percpu.h

**Source**: `cpukit/include/rtems/score/percpu.h` (5.1)

**Status**: active compatibility patch.

**Modified**:

- Under `__FRAMAC__`, changed `_Per_CPU_Information` from an unsized extern
  array to a one-element extern array (`_Per_CPU_Information[1U]`). This
  mirrors the active 6.2 workaround and lets WP reason about
  `_Per_CPU_Information[0]` in the non-SMP proof.
- Added a Frama-C volatile binding for
  `_Per_CPU_Information[0].per_cpu.dispatch_necessary`. The binding writes
  the ghost mirror `_Thread_Dispatch_necessary_ghost`, which scheduler
  contracts use instead of reading the volatile flag in postconditions.

### thread.h

**Source**: `cpukit/include/rtems/score/thread.h` (5.1)

**Status**: active compatibility patch.

**Modified**:

- Commented out the trailing `extensions[RTEMS_ZERO_LENGTH_ARRAY]` flexible
  array member in `Thread_Control` because Frama-C/WP cannot reason about
  it. This mirrors the legacy in-tree 5.1 patch from `setup.sh`.

### scheduleredf.c

**Source**: `cpukit/score/src/scheduleredf.c` (5.1)

**Status**: active contract slice.

**Modified**:

- Forward-declares `tstosbt`/`sbttots`/`sbttotv` under `__FRAMAC__` to satisfy
  FC32's implicit-function-declaration strictness.
- Adds an ACSL contract for `_Scheduler_EDF_Initialize()` (verbatim from 6.2;
  body is identical between 5.1 and 6.2).

### scheduleredfnodeinit.c

**Source**: `cpukit/score/src/scheduleredfnodeinit.c` (5.1)

**Status**: active contract slice.

**Modified**:

- Same FC32 timestamp shim as `scheduleredf.c`.
- Adds an ACSL contract for `_Scheduler_EDF_Node_initialize()` (verbatim
  from 6.2; body is identical).
