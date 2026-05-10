# EDF Scheduler Verification with Abstract RBTree Model

## Summary

This plan describes how to verify the RTEMS 6.2 EDF scheduler against an
abstract model of the RTEMS red-black tree. The RBTree implementation itself is
out of scope for this verification effort. The EDF proofs should rely on
explicit contracts for abstract tree behavior instead of reasoning through the
concrete RBTree code or using ad hoc ghost helper assumptions.

The goal is to prove the EDF scheduler preserves and uses the correct ready
queue semantics:

- ready threads are represented by `Scheduler_EDF_Node` entries in the EDF
  ready tree
- the ready tree is ordered by EDF priority/deadline
- the selected highest-ready thread is the owner of the minimum EDF node
- enqueue, extract, and reschedule operations preserve those abstract facts

## Concrete RBTree Locations

The actual RTEMS 6.2 RBTree code lives in the pristine RTEMS source tree:

- `rtems/src/rtems-6.2/cpukit/include/rtems/score/rbtree.h`
  - public score RBTree types and inline helpers
  - defines `RBTree_Node`, `RBTree_Control`, `_RBTree_Insert_inline()`,
    `_RBTree_Initialize_empty()`, `_RBTree_Initialize_node()`, and related
    accessors
- `rtems/src/rtems-6.2/cpukit/include/rtems/score/rbtreeimpl.h`
  - internal RBTree declarations used by implementation files
- `rtems/src/rtems-6.2/cpukit/include/rtems/score/bsd-tree.h`
  - macro implementation inherited from BSD/FreeBSD-style tree code
  - provides the generated insert/remove/rebalance machinery
- `rtems/src/rtems-6.2/cpukit/score/src/rbtree*.c`
  - out-of-line RBTree operations such as `_RBTree_Minimum()`,
    `_RBTree_Extract()`, `_RBTree_Insert_color()`, `_RBTree_Replace_node()`,
    predecessor/successor, append, prepend, and traversal

For EDF specifically:

- `Scheduler_EDF_Context::Ready` is the EDF ready RBTree.
- `Scheduler_EDF_Node::Node` is the RBTree node embedded in each EDF scheduler
  node.
- EDF helper code that touches the ready tree is in
  `verification/6.2/overlay/cpukit/include/rtems/score/scheduleredfimpl.h`.

There is a second RBTree use in the thread priority aggregation code:

- `Priority_Aggregation::Contributors` stores active priority contributors.
- Release/cancel proofs depend on this tree through `_Thread_Priority_add()`,
  `_Thread_Priority_remove()`, and `_Thread_Priority_changed()`.

Keep these two abstract trees distinct in contracts. The EDF ready queue and
thread priority aggregation queue have different element types and proof
obligations.

## RBTree Abstraction Boundary

Introduce a named abstraction for RBTree behavior instead of relying directly
on helper functions such as `_Helper_RBTree_EDF_Minimum()` as unexplained
ghost state.

The abstraction should cover these operations:

- initialize empty
  - the abstract set of tree members becomes empty
  - the abstract minimum becomes absent
- insert
  - the inserted node becomes a member
  - all previous members remain members
  - ordering is preserved according to the caller-provided EDF ordering
  - the abstract minimum is updated when the inserted node is smaller than all
    previous members
- extract/remove
  - the removed node is no longer a member
  - all other previous members remain members
  - ordering is preserved
  - the abstract minimum is either unchanged or becomes the next-smallest
    remaining member
- minimum
  - returns `NULL` for an empty abstract tree
  - otherwise returns a member whose key is less than or equal to every other
    member key

For EDF, the key is `Scheduler_EDF_Node::priority`. For priority aggregation,
the key is `Priority_Node::priority`.

The first implementation step should be to express these facts as ACSL
predicates, logic functions, and/or ghost state with clear names. Existing
ghost variables such as `g_min_edf_node`, `g_min_priority_node`, and
`g_new_minimum` may be retained temporarily, but they should be documented as
the concrete representation of the abstract model rather than free-floating
assumptions.

## Verification Order

Use the following order so each proof has stable contracts from lower layers.

1. Define the abstract RBTree model and contracts.
   - Attach EDF-ready-tree contracts to `_Scheduler_EDF_Enqueue()`,
     `_Scheduler_EDF_Extract()`, and `_Scheduler_EDF_Get_highest_ready()`.
   - Attach priority-aggregation contracts to the priority aggregation helpers
     used by release/cancel.
   - Avoid verifying `rbtree*.c`; treat those functions as trusted abstract
     operations with explicit contracts.

2. Verify the EDF helper layer.
   - `_Scheduler_EDF_Get_context()`
   - `_Scheduler_EDF_Node_downcast()`
   - `_Scheduler_EDF_Enqueue()`
   - `_Scheduler_EDF_Extract()`
   - `_Scheduler_EDF_Get_highest_ready()`

   This is the core boundary between EDF scheduler logic and the abstract
   ready tree.

3. Verify simple EDF functions.
   - `_Scheduler_EDF_Map_priority()`
   - `_Scheduler_EDF_Unmap_priority()`
   - `_Scheduler_EDF_Initialize()`
   - `_Scheduler_EDF_Node_initialize()`

   These functions establish basic EDF node/context state and priority
   encoding. They should not depend on complex scheduling behavior.

4. Verify uniprocessor scheduler helpers as contract boundaries.
   - `_Scheduler_uniprocessor_Update_heir()`
   - `_Scheduler_uniprocessor_Update_heir_if_preemptible()`
   - `_Scheduler_uniprocessor_Update_heir_if_necessary()`
   - `_Scheduler_uniprocessor_Schedule()`
   - `_Scheduler_uniprocessor_Unblock()`
   - `_Scheduler_uniprocessor_Block()`
   - `_Scheduler_uniprocessor_Yield()`

   These functions are not EDF-specific. Their contracts should state how the
   heir changes based on the candidate thread, current heir, priority, and
   preemptibility. EDF proofs should call these through contracts instead of
   repeatedly inlining scheduler mechanics.

5. Verify EDF ready-queue operations.
   - `_Scheduler_EDF_Unblock()`
     - sets the EDF node priority
     - inserts the node into the abstract ready tree
     - updates the heir if the unblocked thread outranks the current heir and
       the current heir is preemptible
   - `_Scheduler_EDF_Update_priority()`
     - exits early if the thread is not ready
     - exits early if the priority did not change
     - otherwise extracts, updates, reinserts, and reschedules against the
       abstract minimum
   - `_Scheduler_EDF_Schedule()`
     - selects the abstract minimum ready node and delegates heir update
   - `_Scheduler_EDF_Block()`
     - removes the blocked node and, if necessary, selects the abstract
       minimum remaining ready node as the next heir candidate
   - `_Scheduler_EDF_Yield()`
     - removes and reinserts the yielding node, then selects the abstract
       minimum ready node

6. Verify EDF release/cancel last.
   - `_Scheduler_EDF_Release_job()`
   - `_Scheduler_EDF_Cancel_job()`

   These depend primarily on thread priority aggregation rather than the EDF
   ready queue. They should use the priority aggregation RBTree abstraction and
   verified contracts for `_Thread_Priority_add()`,
   `_Thread_Priority_remove()`, and `_Thread_Priority_changed()`.

## Contract Strategy

Use contracts to make the proof boundary explicit:

- Do not inline or verify concrete RBTree implementation files.
- Do not leave helper functions as undocumented proof shortcuts.
- For every abstract RBTree helper, state:
  - required validity and ownership assumptions
  - membership before and after the operation
  - minimum before and after the operation
  - key ordering relation used by the caller
  - frame conditions for what the operation may modify
- Prefer named EDF predicates over raw field assertions repeated across
  contracts.

Suggested predicate names:

- `edf_ready_member(context, node)`
- `edf_ready_ordered(context)`
- `edf_ready_minimum(context, node)`
- `edf_node_key(node) == node->priority`
- `priority_aggregation_member(aggregation, node)`
- `priority_aggregation_minimum(aggregation, node)`

The exact representation may be ACSL logic, ghost variables, or a combination
of both. The important requirement is that every proof obligation depends on a
documented abstract RBTree contract rather than on an unexplained global ghost
value.

## Acceptance Criteria

The abstraction and EDF verification work is complete when:

- EDF proofs no longer require reasoning through `rbtree*.c` or
  `bsd-tree.h` macro bodies.
- Every use of `_RBTree_Insert_inline()`, `_RBTree_Extract()`, and
  `_RBTree_Minimum()` reachable from EDF verification has an explicit abstract
  contract.
- `_Scheduler_EDF_Get_highest_ready()` is specified in terms of the abstract
  EDF ready-tree minimum.
- `_Scheduler_EDF_Unblock()`, `_Scheduler_EDF_Update_priority()`,
  `_Scheduler_EDF_Schedule()`, `_Scheduler_EDF_Block()`, and
  `_Scheduler_EDF_Yield()` are verified against the abstract ready-tree model.
- `_Scheduler_EDF_Release_job()` and `_Scheduler_EDF_Cancel_job()` are verified
  against the priority aggregation abstraction.
- Remaining trusted assumptions are listed in one place and distinguish:
  - abstract RBTree correctness
  - `RTEMS_CONTAINER_OF()` pointer recovery
  - volatile dispatch flag limitations

## Notes

The existing `stubs.h` and `release_cancel_stubs.h` files already contain some
of the raw ingredients for this plan, especially `g_min_edf_node`,
`g_min_priority_node`, and `g_new_minimum`. The issue is not that these ghosts
exist. The issue is that they should be tied to a named abstract model with
clear contracts and documented assumptions.

Treat the RBTree abstraction as a trusted model of the RTEMS RBTree API. The
proof should show that EDF scheduler code uses this API correctly, not that the
API implementation is correct.
