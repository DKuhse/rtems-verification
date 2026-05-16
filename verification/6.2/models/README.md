# Active 6.2 Models

This directory is for verification-only abstract models.

## Active Files

- `edf_ready_set.h` — introduces `edf_ready_set{L}(context)` as an ACSL
  `set<Scheduler_EDF_Node *>` representation of the EDF ready queue contents,
  with pure logic operations for singleton, insert, extract, membership, and
  minimum-node selection.  The model also includes
  `edf_ready_valid_nodes{L}(nodes)` to guard priority comparisons over ready
  nodes, `edf_ready_owners_canonical{L}(nodes)` to state that every ready
  node's owner points back to that node as its home scheduler node, and
  `edf_ready_context_well_formed{L}(context)` as the context-level ready-set
  invariant.  The priority-cache predicate
  `edf_ready_context_cache_consistent{L}(context)` is intentionally separate:
  release/cancel priority propagation can temporarily leave several ready
  nodes with stale EDF caches while the ready set remains structurally
  well-formed.  The two-label
  `edf_priority_cache_consistency_preserved{L1,L2}(nodes)` predicate captures
  the weaker update-priority guarantee that nodes which were already
  cache-consistent remain so.  The ready-set abstraction is derived from
  `context->Ready.rbh_root`, so operations that leave the ready RBTree root
  unchanged preserve the abstract set by ordinary congruence.
- `edf_property.h` — introduces the scheduler-level EDF property that the
  selected thread owns a ready EDF node which is not after every other ready
  EDF node according to the EDF ordering predicate.  It also includes the RTEMS
  heir-state property: the heir must be EDF-earliest unless it is
  non-preemptible.  This model intentionally does not state that a node
  priority represents a particular deadline.
- `priority_aggregation.h` — introduces `priority_contributors{L}(aggregation)`
  as an ACSL `set<Priority_Node *>` representation of
  `Priority_Aggregation::Contributors`.  It mirrors the ready-set model style:
  contributor membership is abstract, insert/extract are pure set operations,
  and minimum-node predicates describe the cached aggregate priority without
  exposing RBTree pointer or color mechanics.  Structural aggregation
  well-formedness is kept separate from cached-minimum consistency so
  release/cancel can model the temporary stale state between changing a
  contributor priority and propagating the new aggregate minimum.  It also
  contains the narrow `priority_node_active{L}(node)` predicate used for the
  release/cancel active-node branch condition.
  The assumed RBTree boundary is the small set of priority aggregation leaf
  helpers in `priorityimpl.h`: `_Priority_Is_empty()`,
  `_Priority_Get_minimum_node()`, `_Priority_Plain_insert()`,
  `_Priority_Plain_extract()`, and `_Priority_Plain_changed()`.
- `thread_priority_updates.h` — introduces the concrete
  `thread_priority_update_pending{L}(queue_context, thread)` predicate over
  the two-slot thread-priority update worklist in `Thread_queue_Context`.
  It avoids an abstract set model because the implementation has exactly two
  update slots.

These are not stubs for missing code. They are intentional abstraction
boundaries used by the active RTEMS 6.2 proof.

## Notes

- Several operation contracts use the same bridge from
  `SCHEDULER_PRIORITY_PURIFY(node->Priority.value)` to
  `((Scheduler_EDF_Node *) node)->Base.Wait.Priority.Node.priority`.  This is
  currently left inline because it keeps the proof obligations transparent to
  WP. Using a predicate for it didn't work. If release/cancel repeats this enough to become noisy, consider
  reintroducing a small shared helper macro for it.
