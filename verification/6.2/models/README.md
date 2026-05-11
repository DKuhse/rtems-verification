# Active 6.2 Models

This directory is for verification-only abstract models.

## Active Files

- `edf_ready_set.h` — introduces `edf_ready_set{L}(context)` as an ACSL
  `set<Scheduler_EDF_Node *>` representation of the EDF ready queue contents,
  with pure logic operations for singleton, insert, extract, membership, and
  minimum-node selection.  The model also includes
  `edf_ready_valid_nodes{L}(nodes)` to guard priority comparisons over ready
  nodes.
- `edf_property.h` — introduces the scheduler-level EDF property that the
  selected thread owns a ready EDF node which is not after every other ready
  EDF node according to the EDF ordering predicate.  It also includes the RTEMS
  heir-state property: the heir must be EDF-earliest unless it is
  non-preemptible.  This model intentionally does not state that a node
  priority represents a particular deadline.

These are not stubs for missing code. They are intentional abstraction
boundaries used by the active RTEMS 6.2 proof.
