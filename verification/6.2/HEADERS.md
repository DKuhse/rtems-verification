# Active RTEMS 6.2 Verification Header Inventory

This file tracks active RTEMS 6.2 verification overlays and stubs.

The legacy hand-port inventory lives at:

- `../../legacy/rtems-6.2-hand-port/verification/6.2/HEADERS.md`

## Current State

The active tree contains RTEMS 6.2 overlays for the first EDF unblock
verification slice plus the first abstract ready-set model. Some files remain
pristine imports, while the scheduler, priority, and thread helper path now has
active ACSL contracts.

Copied from `rtems/src/rtems-6.2-pristine/`:

- `overlay/cpukit/include/rtems/score/scheduleredf.h`
- `overlay/cpukit/include/rtems/score/scheduleredfimpl.h`
- `overlay/cpukit/include/rtems/score/schedulerimpl.h`
- `overlay/cpukit/include/rtems/score/percpu.h`
- `overlay/cpukit/include/rtems/score/priorityimpl.h`
- `overlay/cpukit/include/rtems/score/schedulernodeimpl.h`
- `overlay/cpukit/include/rtems/score/scheduleruniimpl.h`
- `overlay/cpukit/include/rtems/score/thread.h`
- `overlay/cpukit/include/rtems/score/threadimpl.h`
- `overlay/cpukit/include/rtems/score/threadqimpl.h`
- `overlay/cpukit/score/src/scheduleredfunblock.c`
- `overlay/cpukit/score/src/scheduleredfreleasejob.c`
- `overlay/cpukit/score/src/threadchangepriority.c`
- `harnesses/thread-get-priority-harness.c`
- `harnesses/scheduleruni-unblock-harness.c`
- `models/edf_ready_set.h`
- `models/edf_property.h`
- `models/priority_aggregation.h`
- `models/thread_priority_updates.h`

The next implementation step is to connect `_Scheduler_EDF_Enqueue()` and
eventually `_Scheduler_EDF_Extract()` / `_Scheduler_EDF_Get_highest_ready()` to
the abstract ready-set model, then annotate only the EDF scheduler helpers
needed to verify `_Scheduler_EDF_Unblock()`.

## Intended Categories

- `overlay/cpukit/include/rtems/score/` — annotated headers that intentionally
  shadow pristine RTEMS 6.2 headers
- `overlay/cpukit/score/src/` — annotated source files passed directly to
  Frama-C
- `harnesses/` — small translation units used to verify header-only inline
  helpers
- `models/` — verification-only model contracts, especially the abstract RBTree
  boundary
- `headers/` — reserve simplified headers, not part of the active include path
  unless explicitly documented here

## Rule

Every active overlay or stub added here must document:

- which pristine RTEMS file or function it replaces or abstracts
- what behavior is preserved verbatim
- what behavior is changed for verification
- what assumptions are trusted rather than proved

## Baseline Imports

### scheduleredf.h

**Source**: `cpukit/include/rtems/score/scheduleredf.h`

**Status**: active compatibility patch.

**Reason for import**: defines the EDF scheduler context, EDF scheduler node,
operation table wiring, and EDF entry point declarations used by the unblock
slice.

**Modified**:

- Commented out `#include <limits.h>` for Frama-C preprocessing compatibility.
  This matches the legacy 6.2 hand-port workaround.
- Includes `edf_ready_set.h` and `edf_property.h` when `__FRAMAC__` is defined
  so contracts on EDF declarations can use the verification models after
  `Scheduler_EDF_Context` and `Scheduler_EDF_Node` are declared.

**Assumption**:

- `INT_MAX` remains available through the RTEMS toolchain/Frama-C preprocessing
  environment when `SCHEDULER_EDF_MAXIMUM_PRIORITY` is used.

### scheduleredfimpl.h

**Source**: `cpukit/include/rtems/score/scheduleredfimpl.h`

**Status**: active compatibility patch.

**Reason for import**: contains the inline EDF helpers used by
`_Scheduler_EDF_Unblock()`, especially `_Scheduler_EDF_Get_context()`,
`_Scheduler_EDF_Node_downcast()`, and `_Scheduler_EDF_Enqueue()`.

**Modified**:

- Added ACSL contracts for `_Scheduler_EDF_Get_context()` and
  `_Scheduler_EDF_Node_downcast()`. The context contract requires a readable
  scheduler with a valid EDF context, assigns nothing, and returns
  `scheduler->context` cast to `Scheduler_EDF_Context *`. The downcast contract
  requires a `Scheduler_Node *` that is the embedded `Base` member of a valid
  `Scheduler_EDF_Node`, assigns nothing, and returns the enclosing EDF node.
- Added ACSL contracts for `_Scheduler_EDF_Enqueue()`,
  `_Scheduler_EDF_Extract()`, `_Scheduler_EDF_Extract_body()`, and
  `_Scheduler_EDF_Get_highest_ready()`. Enqueue/extract abstract RBTree updates
  through `edf_ready_insert()` and `edf_ready_extract()`, preserve structural
  ready-context well-formedness, expose per-node priority-cache preservation,
  and preserve full context cache consistency when it held on entry.

### schedulerimpl.h

**Source**: `cpukit/include/rtems/score/schedulerimpl.h`

**Status**: active verification patch.

**Reason for import**: directly included by EDF scheduler slices and by
`scheduleruniimpl.h`; provides scheduler helper APIs used by the uniprocessor
scheduler path.

**Modified**:

- Under `__FRAMAC__`, completes `_Scheduler_Table` as a one-entry table for the
  UP proof scope and includes the EDF scheduler model predicates needed by the
  `_Scheduler_Release_job()` wrapper contract.
- Adds a UP-only ACSL contract for `_Scheduler_Release_job()` and pins the
  indirect `scheduler->Operations.release_job` call to
  `_Scheduler_EDF_Release_job()` with `@calls`.

### percpu.h

**Source**: `cpukit/include/rtems/score/percpu.h`

**Status**: active compatibility patch.

**Reason for import**: defines `_Thread_Heir`, `_Thread_Dispatch_necessary`,
and `_Per_CPU_Information`, which are used by the uniprocessor scheduler
contracts.

**Modified**:

- Under `__FRAMAC__`, changed `_Per_CPU_Information` from an unsized extern
  array with the alignment attribute before the type to a one-element extern
  array with the alignment attribute after the declarator. This mirrors the
  legacy 6.2 workaround and lets WP reason about `_Per_CPU_Information[0]` in
  the non-SMP proof. The non-Frama-C path keeps the pristine RTEMS declaration.

### priorityimpl.h

**Source**: `cpukit/include/rtems/score/priorityimpl.h`

**Status**: active compatibility patch.

**Reason for import**: provides `_Priority_Get_priority()`, the final helper
in the non-SMP `_Thread_Get_priority()` path.

**Modified**:

- Added an ACSL contract for `_Priority_Get_priority()` requiring a readable
  aggregation priority, assigning only the result, and returning the pre-call
  `aggregation->Node.priority`.
- Added ACSL frame/result contracts for the local `Priority_Actions` helpers:
  `_Priority_Actions_initialize_empty()`,
  `_Priority_Actions_initialize_one()`, `_Priority_Actions_is_empty()`,
  `_Priority_Actions_move()`, and `_Priority_Actions_add()`. These are used by
  the thread-priority drill-down and do not alter the RBTree abstraction
  boundary.
- Added UP-only ACSL contracts for the priority combinators used by
  `_Thread_Priority_do_perform_actions()`:
  `_Priority_Non_empty_insert()`, `_Priority_Extract_non_empty()`, and
  `_Priority_Changed()`. These contracts sit above the intentionally abstract
  plain/RBTree helpers and prove contributor-set updates, cached-minimum
  repair, and the action-list/scheduler-priority callback shape for the EDF
  thread-priority slice.
- Under `__FRAMAC__`, added a Frama-C-friendly scheduler-node helper for the
  wait-priority aggregation and a forward contract for
  `_Thread_Priority_action_change()` so `@calls` annotations on the combinators
  can use the callback contract.

### schedulernodeimpl.h

**Source**: `cpukit/include/rtems/score/schedulernodeimpl.h`

**Status**: active compatibility patch.

**Reason for import**: provides `_Scheduler_Node_get_priority()`, which is
used by `_Scheduler_EDF_Unblock()` to obtain the scheduler node priority before
purifying and storing it in the EDF node.

**Modified**:

- Added an ACSL contract for `_Scheduler_Node_get_priority()`. The contract
  requires a valid scheduler node, assigns only the scheduler-node priority
  subobject, preserves `node->Priority.value`, and returns its pre-call value.
- Added an ACSL contract for `_Scheduler_Node_set_priority()` used by the
  thread-priority callback drill-down.

### scheduleruniimpl.h

**Source**: `cpukit/include/rtems/score/scheduleruniimpl.h`

**Status**: active compatibility patch.

**Reason for import**: contains `_Scheduler_uniprocessor_Unblock()` and heir
update helpers called by `_Scheduler_EDF_Unblock()`.

**Modified**:

- Added ACSL contracts for `_Scheduler_uniprocessor_Update_heir()`,
  `_Scheduler_uniprocessor_Update_heir_if_necessary()`,
  `_Scheduler_uniprocessor_Update_heir_if_preemptible()`, and
  `_Scheduler_uniprocessor_Unblock()`. These contracts describe when
  `_Thread_Heir` changes to a new thread, when it remains unchanged due to
  equal/non-preemptible cases. `_Scheduler_uniprocessor_Unblock()` expresses
  the current heir priority via the non-SMP home scheduler node path used by
  `_Thread_Get_priority()`. Update branches keep `_Thread_Dispatch_necessary`
  in their assigns clauses because the code writes it, but the current
  EDF-facing contract does not prove dispatch-state postconditions. This
  mirrors the legacy 6.2 port: `_Thread_Dispatch_necessary` is volatile, so
  postconditions about its final value are intentionally omitted.
- Under `__FRAMAC__`, `_Scheduler_uniprocessor_Unblock()` expands the
  preemptible-heir condition locally and calls `_Scheduler_uniprocessor_Update_heir()`
  only in the branch where the heir can actually change. The production RTEMS
  body still calls `_Scheduler_uniprocessor_Update_heir_if_preemptible()`.

**Expected verification result**: `scripts/6.2/verify-scheduleruni-unblock.sh
-wp-model 'Typed+Cast' -wp-timeout 30` proves all goals for the helper harness.
The volatile dispatch flag remains in frame clauses, but there is no
postcondition about its final value.

### thread.h

**Source**: `cpukit/include/rtems/score/thread.h`

**Status**: pristine copy, no verification changes.

**Reason for import**: directly included by `scheduleredfunblock.c` and needed
for `Thread_Control` layout used by the unblock contract.

### threadimpl.h

**Source**: `cpukit/include/rtems/score/threadimpl.h`

**Status**: active compatibility patch.

**Reason for import**: provides `_Thread_Get_priority()` and
`_Thread_Scheduler_get_home_node()`, which connect the uniprocessor unblock
priority comparison to the non-SMP home scheduler node.

**Modified**:

- Added a non-SMP ACSL contract for `_Thread_Scheduler_get_home_node()`,
  returning `the_thread->Scheduler.nodes`.
- Added an ACSL contract for `_Thread_Get_priority()`, requiring readable
  scheduler-node state and returning the pre-call home-node wait priority.
- Added non-SMP ACSL contracts for `_Thread_Priority_add()`,
  `_Thread_Priority_remove()`, and `_Thread_Priority_changed()`, used by EDF
  release/cancel to describe contributor-set changes, cached-minimum repair,
  and priority-update queuing. `_Thread_Priority_changed()` explicitly requires
  a valid `Priority_Group_order` value.

### threadqimpl.h

**Source**: `cpukit/include/rtems/score/threadqimpl.h`

**Status**: active compatibility patch.

**Reason for import**: provides `_Thread_queue_Context_add_priority_update()`,
the inline helper called by `_Thread_Priority_do_perform_actions()` when a
priority action changes a scheduler node priority and the affected thread must
be queued for `_Thread_Priority_update()`.

**Modified**:

- Added an ACSL contract for `_Thread_queue_Context_add_priority_update()`.
  The contract frames the helper to `Priority.update_count` and the selected
  two-slot update entry, records that the current thread pointer is stored, and
  preserves `Priority.Actions.actions`. This lets the
  `_Thread_Priority_do_perform_actions()` do-nothing-callback scaffold prove
  that the callback target and pending action list are not clobbered by the
  update-list helper.

### threadchangepriority.c

**Source**: `cpukit/score/src/threadchangepriority.c`

**Status**: active contract slice.

**Reason for import**: contains `_Thread_Priority_add()`,
`_Thread_Priority_remove()`, and `_Thread_Priority_changed()`, which are the
thread-priority operations called by EDF release/cancel.

**Modified**:

- Under `__FRAMAC__`, forward-declares timestamp helpers reachable through
  `timestampimpl.h`, matching the other active FC 32 slices.
- Adds an ACSL contract for the internal `_Thread_Priority_apply()` layer. The
  public operations in `threadimpl.h` are verified against this contract by
  `scripts/6.2/verify-thread-change-priority.sh`. Priority RBTree/plain
  helpers remain permanently abstract and out of scope.
- Adds ACSL contracts for `_Thread_Set_scheduler_node_priority()` and
  `_Thread_Priority_action_change()`. The code body remains the RTEMS
  `SCHEDULER_NODE_OF_WAIT_PRIORITY_NODE()` expression; the Frama-C-only
  scheduler-node helper is used only in ACSL annotations.
- Adds a first-pass non-SMP scaffold contract for
  `_Thread_Priority_do_perform_actions()` under the
  `_Thread_queue_Do_nothing_priority_actions` callback. The scaffold currently
  proves that the local one-action add/remove/change path drains the priority
  action list; contributor-set and cached-minimum postconditions are intended
  to be added back incrementally.

### scheduleredfreleasejob.c

**Source**: `cpukit/score/src/scheduleredfreleasejob.c`

**Status**: active contract slice.

**Reason for import**: contains `_Scheduler_EDF_Release_job()` and
`_Scheduler_EDF_Cancel_job()`, the EDF release/cancel entry points that drive
the thread-priority add/remove/changed chain.

**Modified**:

- Added ACSL contracts for EDF release/cancel and for the generic
  `_Scheduler_Release_job()` wrapper.
- Added local proof assertions preserving EDF ready-set canonical ownership
  across the thread-priority propagation call.

### scheduleredfunblock.c

**Source**: `cpukit/score/src/scheduleredfunblock.c`

**Status**: pristine copy, no verification changes.

**Reason for import**: first active EDF verification target for the new 6.2
port.

### scheduleruni-unblock-harness.c

**Source**: new verification-only harness.

**Status**: active harness.

**Reason for import**: includes `scheduleruniimpl.h` as a translation unit so
Frama-C/WP can verify the header-only uniprocessor scheduler inline helpers.

**Behavior preserved**: no runtime behavior is added. The harness only includes
the RTEMS uniprocessor scheduler implementation header.

### thread-get-priority-harness.c

**Source**: new temporary verification-only harness.

**Status**: temporary active harness.

**Reason for import**: includes `threadimpl.h` as a small translation unit so
Frama-C/WP can isolate `_Thread_Get_priority()` and the direct helper contracts
it depends on.

**Behavior preserved**: no runtime behavior is added. The harness only includes
the RTEMS thread implementation header.

### edf_ready_set.h

**Source**: new verification-only model header.

**Status**: active abstract model.

**Reason for import**: introduces `edf_ready_set{L}(context)` as the ACSL
representation function for the scheduler-facing contents of
`Scheduler_EDF_Context::Ready`.

**Definitions**:

- `logic set<Scheduler_EDF_Node *> edf_ready_set{L}(context)`
- `predicate edf_ready_member{L}(context, node)`
- `predicate edf_ready_empty{L}(context)`
- `predicate edf_ready_set_member(nodes, node)`
- `predicate edf_ready_valid_nodes{L}(nodes)`
- `predicate edf_ready_node_has_canonical_owner{L}(node)`
- `predicate edf_ready_owners_canonical{L}(nodes)`
- `predicate edf_ready_context_well_formed{L}(context)`
- `predicate edf_ready_node_cache_consistent{L}(node)`
- `predicate edf_priority_cache_consistent{L}(nodes)`
- `predicate edf_ready_context_cache_consistent{L}(context)`
- `predicate edf_priority_cache_consistency_preserved{L1,L2}(nodes)`
- `logic set<Scheduler_EDF_Node *> edf_ready_singleton(node)`
- `logic set<Scheduler_EDF_Node *> edf_ready_insert(nodes, node)`
- `logic set<Scheduler_EDF_Node *> edf_ready_extract(nodes, node)`
- `predicate edf_ready_minimum_node{L}(nodes, node)`
- `predicate edf_ready_minimum{L}(context, node)`

`edf_ready_set{L}(context)` has a `reads` clause over `context->Ready` and
the embedded `Scheduler_EDF_Node::Node` fields of valid EDF nodes. This makes
the representation function explicitly depend on the concrete ready-tree state
without exposing RBTree shape in scheduler contracts.

The set operations are pure logic functions over `set<Scheduler_EDF_Node *>`.
They are intended for contracts such as:

- enqueue: `edf_ready_set{Here}(context) ==
  edf_ready_insert(edf_ready_set{Pre}(context), node)`
- extract: `edf_ready_set{Here}(context) ==
  edf_ready_extract(edf_ready_set{Pre}(context), node)`
- highest ready: result owner comes from a node satisfying
  `edf_ready_minimum{Here}(context, node)`

`edf_ready_minimum_node{L}(nodes, node)` requires
`edf_ready_valid_nodes{L}(nodes)` so priority comparisons only range over valid
EDF scheduler nodes.

`edf_ready_context_well_formed{L}(context)` requires a valid context, valid
ready nodes, owner-distinct ready nodes, and canonical ready-node ownership:
each ready node's owner points back to that node as its home scheduler node.
It is the structural context-level invariant used by scheduler helper contracts.

`edf_ready_context_cache_consistent{L}(context)` is a separate invariant saying
that every ready node's cached EDF `priority` agrees with its scheduler
aggregation priority.  It is intentionally not folded into well-formedness:
release/cancel priority propagation may repair several ready nodes in a loop, so
the ready set can remain structurally well-formed while some EDF caches are
temporarily stale.

`edf_priority_cache_consistency_preserved{L1,L2}(nodes)` is the frame-style
form used by `_Scheduler_EDF_Update_priority()`: any node in `nodes` whose cache
was consistent at label `L1` remains cache-consistent at label `L2`, without
requiring every node in `nodes` to be consistent at `L1`.

**Current scope**: contents only. This model intentionally does not describe
RBTree shape, colors, rotations, or traversal.

**Expected next changes**: add contracts for `_Scheduler_EDF_Extract()` and
`_Scheduler_EDF_Get_highest_ready()` in terms of the ready-set model.

### edf_property.h

**Source**: new verification-only model header.

**Status**: active abstract model.

**Reason for import**: introduces the scheduler-level EDF property over the
abstract ready set.

**Definitions**:

- `predicate edf_ready_earliest_node{L}(nodes, node)`
- `predicate edf_ready_node_not_after{L}(left, right)`
- `predicate edf_thread_owns_earliest_ready_node{L}(nodes, heir)`
- `predicate edf_thread_is_earliest_ready{L}(context, thread)`
- `predicate edf_preemptible_heir_is_earliest_ready{L}(context, heir)`

`edf_thread_is_earliest_ready{L}(context, thread)` states that `thread` owns a
ready EDF node which satisfies `edf_ready_node_not_after{L}(node, other)` for
every other ready EDF node in `edf_ready_set{L}(context)`.

`edf_preemptible_heir_is_earliest_ready{L}(context, heir)` is the RTEMS
heir-state property: if `heir` is preemptible, then it must be EDF-earliest;
if it is not preemptible, RTEMS may intentionally leave it as the heir even
when another ready thread has an earlier EDF priority.

**Current scope**: EDF ordering over ready nodes and the scheduler heir. This
model intentionally does not state that a priority value represents a
particular deadline and does not model equal-priority FIFO/tie order.
