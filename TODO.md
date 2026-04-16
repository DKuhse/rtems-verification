# RTEMS 6.2 Verification Port — Task Tracker

## Infrastructure
- [x] Docker toolchain image (cross-compiler + Frama-C 25)
- [x] RTEMS 6.2 source downloaded and extracted to `rtems/src/rtems-6.2/`
- [x] Frama-C patches applied (limits.h, Per_CPU_Information[1U], thread.h flexible arrays)
- [x] 5.1 cross-compiler confirmed working for 6.2 preprocessing
- [x] Basic verification working (Map/Unmap: 4/4)
- [ ] Update `setup.sh` to handle 6.2 (patches + file copy)
- [ ] Create `scripts/6.2/verify-wp-all.sh` headless verification script
- [ ] Create per-target `scripts/6.2/verify-*.sh` scripts

## Annotated Headers
These replace the originals in the 6.2 source tree. Required because
inline functions need ACSL contracts, and the stubs reference types
defined in these headers.

- [ ] `priorityimpl.h` — mechanical bool→enum changes throughout:
  - [ ] Ghost variables (`g_new_minimum`, `g_min_priority_node`)
  - [ ] `_Priority_Actions_initialize_empty` — unchanged
  - [ ] `_Priority_Actions_initialize_one` — unchanged
  - [ ] `_Priority_Actions_is_empty` — unchanged
  - [ ] `_Priority_Actions_is_valid` — REMOVED in 6.2 (delete contract)
  - [ ] `_Priority_Actions_move` — unchanged
  - [ ] `_Priority_Actions_add` — unchanged
  - [ ] `_Priority_Node_set_priority` — unchanged
  - [ ] `_Priority_Node_set_inactive` — unchanged
  - [ ] `_Priority_Node_is_active` — unchanged
  - [ ] `_Priority_Get_priority` — unchanged
  - [ ] `_Priority_Get_minimum_node` — unchanged (uses ghost `g_min_priority_node`)
  - [ ] `_Priority_Set_action_node` — unchanged
  - [ ] `_Priority_Set_action_type` — unchanged
  - [ ] `_Priority_Set_action` — unchanged
  - [ ] `_Priority_Get_next_action` — unchanged
  - [ ] `_Priority_Plain_insert` — unchanged (uses ghost `g_new_minimum`)
  - [ ] `_Priority_Plain_extract` — unchanged
  - [ ] `_Priority_Plain_changed` — unchanged
  - [ ] `_Priority_Change_nothing` — bool→`Priority_Group_order`
  - [ ] `_Priority_Remove_nothing` — unchanged
  - [ ] `_Priority_Non_empty_insert` — `false`→`PRIORITY_GROUP_LAST` in callback
  - [ ] `_Priority_Extract_non_empty` — `true`→`PRIORITY_GROUP_FIRST` in callback
  - [ ] `_Priority_Changed` — `bool prepend_it`→`Priority_Group_order group_order`
  - [ ] `_Priority_Replace` — unchanged
  - [ ] Typedef changes: `Priority_Change_handler` uses `Priority_Group_order`

- [ ] `scheduleredfimpl.h`:
  - [ ] Ghost variable `g_edf_sched_context`
  - [ ] `_Scheduler_EDF_Get_context` — add `\from` clause
  - [ ] `_Scheduler_EDF_Thread_get_node` — unchanged
  - [ ] `_Scheduler_EDF_Node_downcast` — add `\from` clause
  - [ ] `_Scheduler_EDF_Enqueue` — `assigns \nothing`
  - [ ] `_Scheduler_EDF_Extract` — `assigns \nothing`
  - [ ] `_Scheduler_EDF_Get_highest_ready` — NEW: ghost `g_min_edf_node`, returns `g_min_edf_node->Base.owner`

- [ ] `schedulerimpl.h`:
  - [ ] `_Scheduler_Get_context` — unchanged
  - [ ] Remove `_Scheduler_Update_heir` (moved to scheduleruniimpl.h)
  - [ ] Other functions as needed by include chain

- [ ] `scheduleruniimpl.h` — NEW file, annotate for inlining:
  - [ ] `_Scheduler_uniprocessor_Update_heir` — assigns `_Thread_Heir`, volatile `dispatch_necessary` limitation
  - [ ] `_Scheduler_uniprocessor_Update_heir_if_necessary` — wraps above
  - [ ] `_Scheduler_uniprocessor_Update_heir_if_preemptible` — wraps with preemptible check
  - [ ] `_Scheduler_uniprocessor_Block` — extract + schedule callback
  - [ ] `_Scheduler_uniprocessor_Unblock` — priority comparison + heir update
  - [ ] `_Scheduler_uniprocessor_Schedule` — callback + heir update
  - [ ] `_Scheduler_uniprocessor_Yield` — callback + unconditional heir update

## Stubs
- [x] `release_cancel_stubs.h` — created, includes `priorityimpl.h` for type
  - [ ] Finalize once annotated `priorityimpl.h` is in place (currently conflicts with unannotated original)
- [ ] `stubs.h` — adapt from 5.1:
  - [ ] Ghost variables (same as 5.1)
  - [ ] `_Helper_RBTree_Minimum` — add `\from`
  - [ ] `_Helper_RBTree_EDF_Minimum` — add `\from`
  - [ ] `_Helper_SCHEDULER_NODE_OF_WAIT_PRIORITY_NODE` — add `\from`
  - [ ] `_Thread_queue_Do_nothing_priority_actions` — unchanged
  - [ ] `_Thread_queue_Context_add_priority_update` — unchanged
  - [ ] `_Scheduler_Node_set_priority` — bool→`Priority_Group_order`
  - [ ] `_Scheduler_Node_get_priority` — unchanged
  - [ ] `_Thread_Get_CPU` — `assigns \result \from thread`
  - [ ] `_Thread_Update_CPU_time_used` — unchanged
  - [ ] `_States_Is_ready` / `_Thread_Is_ready` — unchanged
  - [ ] `_Thread_Get_priority` — unchanged
  - [ ] `_Thread_Scheduler_get_home_node` — add `\from`
  - [ ] Remove `PRIORITY_PSEUDO_ISR` references (removed in 6.2)

## Annotated Source Files (.c)
- [x] `scheduleredfreleasejob.c` — created, `false`→`PRIORITY_GROUP_LAST` in Release_job
  - [ ] Verify once annotated headers are in place
- [ ] `scheduleredfchangepriority.c`:
  - [ ] Port outer contract for `_Scheduler_EDF_Update_priority`
  - [ ] Update `-inline-calls` for uniprocessor layer chain
  - [ ] Add `\separated` with `_Per_CPU_Information`
- [ ] `scheduleredfunblock.c`:
  - [ ] Port outer contract for `_Scheduler_EDF_Unblock`
  - [ ] Update `-inline-calls` for uniprocessor layer
  - [ ] Add `\separated` with `_Per_CPU_Information`
- [ ] `threadchangepriority.c`:
  - [ ] `_Thread_Set_scheduler_node_priority` — bool→enum
  - [ ] `_Thread_Priority_action_change` — bool→enum
  - [ ] `_Thread_Priority_do_perform_actions` — bool→enum, loop structure
  - [ ] `_Thread_Priority_apply` — bool→enum, SMP path changes (non-SMP path similar)
  - [ ] `_Thread_Priority_add` — `false`→`PRIORITY_GROUP_LAST`
  - [ ] `_Thread_Priority_remove` — `true`→`PRIORITY_GROUP_FIRST`
  - [ ] `_Thread_Priority_changed` — bool→enum

## Verification Targets
Goal: all functions verified with `-wp-model "Typed+Cast"`.

- [ ] `_Scheduler_EDF_Map_priority` — 2/2 on 5.1, verified 4/4 with Unmap on 6.2
- [ ] `_Scheduler_EDF_Unmap_priority` — 2/2 on 5.1, verified 4/4 with Map on 6.2
- [ ] `_Scheduler_EDF_Cancel_job` — 33/33 on 5.1
- [ ] `_Scheduler_EDF_Release_job` — 40/40 on 5.1
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
- [x] `_Scheduler_EDF_Update_priority` — **96/98** on 6.2 (was 99/99 on 5.1)
      2 remaining: 1 requires (callback through deeper inline chain),
      1 assigns_normal (no \from on _Thread_Heir pointer assignment in
      inlined _Scheduler_uniprocessor_Update_heir — same root cause as
      5.1's volatile _Thread_Dispatch_necessary issue).
- [x] `_Scheduler_EDF_Unblock` — **74/75** on 6.2 (was 69/69 on 5.1)
      1 remaining: assigns_normal (same \from / Per_CPU issue as above).
      Also fixed: CPU_STRUCTURE_ALIGNMENT attribute position in percpu.h
      needed to be moved from type to variable for WP type matching.

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
