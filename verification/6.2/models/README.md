# Active 6.2 Models

This directory is for verification-only abstract models.

## Active Files

- `edf_ready_model.h` — introduces `edf_ready_nodes{L}(context)` as an ACSL
  `set<Scheduler_EDF_Node *>` representation of the EDF ready queue contents,
  with pure logic operations for singleton, insert, extract, membership, and
  minimum-node selection.

These are not stubs for missing code. They are intentional abstraction
boundaries used by the active RTEMS 6.2 proof.
