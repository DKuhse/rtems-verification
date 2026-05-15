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
  cache-consistent remain so.
- `edf_property.h` — introduces the scheduler-level EDF property that the
  selected thread owns a ready EDF node which is not after every other ready
  EDF node according to the EDF ordering predicate.  It also includes the RTEMS
  heir-state property: the heir must be EDF-earliest unless it is
  non-preemptible.  This model intentionally does not state that a node
  priority represents a particular deadline.

These are not stubs for missing code. They are intentional abstraction
boundaries used by the active RTEMS 6.2 proof.

## Notes

- Several operation contracts use the same bridge from
  `SCHEDULER_PRIORITY_PURIFY(node->Priority.value)` to
  `((Scheduler_EDF_Node *) node)->Base.Wait.Priority.Node.priority`.  This is
  currently left inline because it keeps the proof obligations transparent to
  WP. Using a predicate for it didn't work. If release/cancel repeats this enough to become noisy, consider
  reintroducing a small shared helper macro for it.
