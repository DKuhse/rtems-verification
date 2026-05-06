# RTEMS 6.2 Verification Port — Task Tracker

## Infrastructure
- [x] Docker toolchain image (cross-compiler + Frama-C 25)
- [x] RTEMS 6.2 source downloaded and extracted to `rtems/src/rtems-6.2/`
- [x] Frama-C patches applied (limits.h, Per_CPU_Information[1U] + CPU_STRUCTURE_ALIGNMENT, thread.h flexible arrays)
- [x] 5.1 cross-compiler confirmed working for 6.2 preprocessing
- [x] Basic verification working (Map/Unmap: 4/4)
- [x] Update `setup.sh` to handle 6.2 (patches + file copy)
- [x] Create `scripts/6.2/verify-wp-all.sh` headless verification script
- [x] Create per-target `scripts/6.2/verify-*.sh` scripts (`verify-edf-update-priority.sh`, `verify-edf-unblock.sh`, `verify-thread-priority.sh`; `verify-edf-release-cancel.sh` already existed)
- [x] `verify-6.2` service in docker-compose.yml

## Annotated Headers
These replace the originals in the 6.2 source tree. Required because
inline functions need ACSL contracts, and the stubs reference types
defined in these headers.

- [x] `priorityimpl.h` — mechanical bool→enum changes throughout:
  - [x] Ghost variables (`g_new_minimum`, `g_min_priority_node`)
  - [x] `_Priority_Actions_initialize_empty` — unchanged
  - [x] `_Priority_Actions_initialize_one` — unchanged
  - [x] `_Priority_Actions_is_empty` — unchanged
  - [x] `_Priority_Actions_is_valid` — REMOVED in 6.2 (delete contract)
  - [x] `_Priority_Actions_move` — unchanged
  - [x] `_Priority_Actions_add` — unchanged
  - [x] `_Priority_Node_set_priority` — unchanged
  - [x] `_Priority_Node_set_inactive` — unchanged
  - [x] `_Priority_Node_is_active` — unchanged
  - [x] `_Priority_Get_priority` — unchanged
  - [x] `_Priority_Get_minimum_node` — unchanged (uses ghost `g_min_priority_node`)
  - [x] `_Priority_Set_action_node` — unchanged
  - [x] `_Priority_Set_action_type` — unchanged
  - [x] `_Priority_Set_action` — unchanged
  - [ ] `_Priority_Get_next_action` — unchanged (no contract in file, not used by verified functions)
  - [x] `_Priority_Plain_insert` — unchanged (uses ghost `g_new_minimum`)
  - [x] `_Priority_Plain_extract` — unchanged
  - [x] `_Priority_Plain_changed` — unchanged
  - [x] `_Priority_Change_nothing` — bool→`Priority_Group_order`
  - [x] `_Priority_Remove_nothing` — unchanged
  - [x] `_Priority_Non_empty_insert` — `false`→`PRIORITY_GROUP_LAST` in callback
  - [x] `_Priority_Extract_non_empty` — `true`→`PRIORITY_GROUP_FIRST` in callback
  - [x] `_Priority_Changed` — `bool prepend_it`→`Priority_Group_order group_order`
  - [ ] `_Priority_Replace` — unchanged (no contract in file, not used by verified functions)
  - [x] Typedef changes: `Priority_Change_handler` uses `Priority_Group_order`

- [x] `scheduleredfimpl.h`:
  - [x] Ghost variable `g_edf_sched_context`
  - [x] `_Scheduler_EDF_Get_context` — add `\from` clause
  - [ ] `_Scheduler_EDF_Thread_get_node` — unchanged (no contract in file, not used by verified functions)
  - [x] `_Scheduler_EDF_Node_downcast` — add `\from` clause
  - [x] `_Scheduler_EDF_Enqueue` — `assigns \nothing`
  - [x] `_Scheduler_EDF_Extract` — `assigns \nothing`
  - [x] `_Scheduler_EDF_Get_highest_ready` — NEW: ghost `g_min_edf_node`, returns `g_min_edf_node->Base.owner`

- [ ] `schedulerimpl.h` — not created; not needed for current verification targets
  - [ ] `_Scheduler_Get_context` — unchanged
  - [ ] Remove `_Scheduler_Update_heir` (moved to scheduleruniimpl.h)
  - [ ] Other functions as needed by include chain

- [ ] `scheduleruniimpl.h` — NEW file, annotate for inlining:
  - [x] `_Scheduler_uniprocessor_Update_heir` — assigns `_Thread_Heir`, volatile `dispatch_necessary` limitation
  - [ ] `_Scheduler_uniprocessor_Update_heir_if_necessary` — wraps above (no contract, not used)
  - [x] `_Scheduler_uniprocessor_Update_heir_if_preemptible` — wraps with preemptible check
  - [ ] `_Scheduler_uniprocessor_Block` — extract + schedule callback (no contract, inlined as-is)
  - [ ] `_Scheduler_uniprocessor_Unblock` — priority comparison + heir update (no contract, inlined as-is)
  - [ ] `_Scheduler_uniprocessor_Schedule` — callback + heir update (no contract, inlined as-is)
  - [ ] `_Scheduler_uniprocessor_Yield` — callback + unconditional heir update (no contract, not used)

## Stubs
- [x] `release_cancel_stubs.h` — created, includes `priorityimpl.h` for type
  - [x] Finalize once annotated `priorityimpl.h` is in place
- [x] `stubs.h` — adapt from 5.1:
  - [x] Ghost variables (same as 5.1)
  - [x] `_Helper_RBTree_Minimum` — add `\from`
  - [x] `_Helper_RBTree_EDF_Minimum` — add `\from`
  - [x] `_Helper_SCHEDULER_NODE_OF_WAIT_PRIORITY_NODE` — add `\from`
  - [x] `_Thread_queue_Do_nothing_priority_actions` — unchanged
  - [x] `_Thread_queue_Context_add_priority_update` — unchanged
  - [x] `_Scheduler_Node_set_priority` — bool→`Priority_Group_order`
  - [x] `_Scheduler_Node_get_priority` — unchanged
  - [x] `_Thread_Get_CPU` — `assigns \result \from thread`
  - [x] `_Thread_Update_CPU_time_used` — unchanged
  - [x] `_States_Is_ready` / `_Thread_Is_ready` — unchanged
  - [x] `_Thread_Get_priority` — unchanged
  - [x] `_Thread_Scheduler_get_home_node` — add `\from`
  - [x] Remove `PRIORITY_PSEUDO_ISR` references (removed in 6.2)

## Annotated Source Files (.c)
- [x] `scheduleredfreleasejob.c` — created, `false`→`PRIORITY_GROUP_LAST` in Release_job
  - [x] Verify once annotated headers are in place
- [x] `scheduleredfchangepriority.c`:
  - [x] Port outer contract for `_Scheduler_EDF_Update_priority`
  - [x] Update `-inline-calls` for uniprocessor layer chain
  - [x] Add `\separated` with `_Per_CPU_Information`
- [x] `scheduleredfunblock.c`:
  - [x] Port outer contract for `_Scheduler_EDF_Unblock`
  - [x] Update `-inline-calls` for uniprocessor layer
  - [x] Add `\separated` with `_Per_CPU_Information`
- [x] `threadchangepriority.c`:
  - [x] `_Thread_Set_scheduler_node_priority` — bool→enum
  - [x] `_Thread_Priority_action_change` — bool→enum
  - [x] `_Thread_Priority_do_perform_actions` — bool→enum, loop structure
  - [x] `_Thread_Priority_apply` — bool→enum, SMP path changes (non-SMP path similar)
  - [x] `_Thread_Priority_add` — `false`→`PRIORITY_GROUP_LAST`
  - [x] `_Thread_Priority_remove` — `true`→`PRIORITY_GROUP_FIRST`
  - [x] `_Thread_Priority_changed` — bool→enum

## Verification Targets
Goal: all functions verified with `-wp-model "Typed+Cast"`.

- [x] `_Scheduler_EDF_Map_priority` — **2/2** on 6.2
- [x] `_Scheduler_EDF_Unmap_priority` — **2/2** on 6.2
- [x] `_Scheduler_EDF_Cancel_job` — **33/33** on 6.2
- [x] `_Scheduler_EDF_Release_job` — **40/40** on 6.2
- [x] `_Thread_Set_scheduler_node_priority` — **16/16** on 6.2
- [x] `_Thread_Priority_action_change` — **15/15** on 6.2
- [x] `_Thread_Priority_do_perform_actions` — **993/993** on 6.2
- [x] `_Thread_Priority_apply` — **1633/1633** on 6.2
- [x] `_Thread_Priority_add` — **558/558** on 6.2
- [x] `_Thread_Priority_remove` — **560/560** on 6.2
- [x] `_Thread_Priority_changed` — **781/781** on 6.2

  Root cause identified: WP splits postconditions into `_partN`
  sub-goals. In 5.1, each ensures produces 12 parts (one per behavior
  combination). In 6.2, the SAME contracts produce only 3 parts.
  Fewer parts = each covers more code paths = too complex for Qed.

  This is NOT caused by the enum change — it persists regardless of
  behavior count. WP generates parts based on the preprocessed code
  structure, and RTEMS 6.2's different type layouts (struct sizes,
  field offsets) cause WP to partition differently.

  Adding `requires priority_group_order == PRIORITY_GROUP_FIRST ||
  priority_group_order == PRIORITY_GROUP_LAST` to all enum-parameter
  contracts fixed `complete behaviors` goals. But the `_partN`
  ensures goals remain at ~85% proof rate.

  **Key finding: `-wp-split` resolves the partitioning issue.**
  Without it, WP generates 3 coarse parts per ensures (too complex).
  With `-wp-split`, WP generates 12 fine-grained parts (matching
  5.1's structure). Result: 814/915 on do_perform_actions (up from
  345/406).

  101 remaining timeouts at 30s are structurally harder in 6.2 due
  to different type layouts. Options:
  - Install Z3 in the Docker image as an additional prover
  - Accept ~89% and document remaining as known timeouts
  - Investigate specific failing goals for contract improvements
- [x] `_Scheduler_EDF_Update_priority` — **97/97** on 6.2 (was 99/99 on 5.1)
- [x] `_Scheduler_EDF_Unblock` — **74/74** on 6.2 (was 69/69 on 5.1)

      Fixes that got us to 100%:
      1. Added `_Thread_Dispatch_necessary` to `assigns` clause of
         `exec_update_new_h` — the inlined `_Scheduler_uniprocessor_Update_heir`
         writes to it, so the outer contract must list it (even though
         the `ensures` about its value is unprovable due to volatile).
      2. Added explicit `requires \valid(g_min_edf_node)` — the
         `_Scheduler_EDF_Get_highest_ready` callback's precondition
         wasn't implied by the existing `\valid(g_min_edf_node->Base.owner)`
         in WP's strict checking.
      3. Moved `CPU_STRUCTURE_ALIGNMENT` attribute position in percpu.h
         from type to variable (so WP type matching works with our
         `\separated` clauses).

## Blocking Issue: Excessive inline function visibility in 6.2

RTEMS 6.2 headers expose **450 `static inline` functions** after
preprocessing vs **50 in 5.1**. Each uncontracted function gets
`assigns \everything` by default, which clobbers WP's proof context.

Current result: 68/77 on Release/Cancel (9 Degenerated goals).
The Degenerated goals correspond to requires that WP can't establish
because the `assigns \everything` from uncontracted inline functions
makes the memory state unpredictable.

The root cause: the `release_cancel_stubs.h` includes
`<rtems/score/priorityimpl.h>` (needed for `Priority_Group_order` type),
which pulls in `<rtems/score/scheduler.h>` and transitively most of
the RTEMS inline function universe.

Current state: **77/77 on Release/Cancel** (matching 5.1).
Solved: flexible arrays (`RTEMS_ZERO_LENGTH_ARRAY`) in thread.h caused
`Undefined array-size` warnings that made WP degenerate `\valid` goals.
Patching out lines 641 and 1009 of thread.h fixed all 9 Degenerated goals.

Simplified headers (`schedulerimpl.h`, `threadimpl.h`) were created
during debugging but turned out to be unnecessary for Release/Cancel.
WP only analyzes functions targeted by `-wp-fct`, so the 450
uncontracted inline functions from original headers don't interfere.

The simplified headers are kept in `verification/6.2/headers/` in case
they're needed for Phase 3 (Update_priority, Unblock), where
`-inline-calls` pulls in `scheduleruniimpl.h` functions that call
`_Thread_Get_CPU`, `_Thread_Update_CPU_time_used`, etc. But they should
only be deployed if actually needed — don't simplify headers
preemptively.

## Notes
- 5.1 cross-compiler works for 6.2 preprocessing (no need for RTEMS 6 toolchain)
- Only 2 patches needed for 6.2 (limits.h + Per_CPU_Information), vs 3 for 5.1
- `Priority_Group_order` enum: `PRIORITY_GROUP_FIRST=0`, `PRIORITY_GROUP_LAST=1` — arithmetic identical to 5.1's `bool` (`true=prepend=0`, `false=append=1`)
