# Active RTEMS 6.2 Verification Header Inventory

This file tracks active RTEMS 6.2 verification overlays and stubs.

The legacy hand-port inventory lives at:

- `../../legacy/rtems-6.2-hand-port/verification/6.2/HEADERS.md`

## Current State

The active tree contains pristine RTEMS 6.2 copies for the first EDF unblock
verification slice plus the first abstract ready-set model. The copied RTEMS
files are baseline imports only. They do not yet contain active ACSL contracts.

Copied from `rtems/src/rtems-6.2-pristine/`:

- `overlay/cpukit/include/rtems/score/scheduleredf.h`
- `overlay/cpukit/include/rtems/score/scheduleredfimpl.h`
- `overlay/cpukit/include/rtems/score/schedulerimpl.h`
- `overlay/cpukit/include/rtems/score/scheduleruniimpl.h`
- `overlay/cpukit/include/rtems/score/thread.h`
- `overlay/cpukit/score/src/scheduleredfunblock.c`
- `models/edf_ready_set.h`
- `models/edf_property.h`

The next implementation step is to connect `_Scheduler_EDF_Enqueue()` and
eventually `_Scheduler_EDF_Extract()` / `_Scheduler_EDF_Get_highest_ready()` to
the abstract ready-set model, then annotate only the EDF scheduler helpers
needed to verify `_Scheduler_EDF_Unblock()`.

## Intended Categories

- `overlay/cpukit/include/rtems/score/` — annotated headers that intentionally
  shadow pristine RTEMS 6.2 headers
- `overlay/cpukit/score/src/` — annotated source files passed directly to
  Frama-C
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
- Includes `edf_ready_set.h` and `edf_property.h` when `__FRAAMC__` is defined
  so contracts on EDF declarations can use the verification models after
  `Scheduler_EDF_Context` and `Scheduler_EDF_Node` are declared.

**Assumption**:

- `INT_MAX` remains available through the RTEMS toolchain/Frama-C preprocessing
  environment when `SCHEDULER_EDF_MAXIMUM_PRIORITY` is used.

### scheduleredfimpl.h

**Source**: `cpukit/include/rtems/score/scheduleredfimpl.h`

**Status**: pristine copy, no verification changes.

**Reason for import**: contains the inline EDF helpers used by
`_Scheduler_EDF_Unblock()`, especially `_Scheduler_EDF_Get_context()`,
`_Scheduler_EDF_Node_downcast()`, and `_Scheduler_EDF_Enqueue()`.

**Expected verification changes**: add contracts around the EDF helper layer
and connect `_Scheduler_EDF_Enqueue()` to the abstract EDF ready-tree model.

### schedulerimpl.h

**Source**: `cpukit/include/rtems/score/schedulerimpl.h`

**Status**: pristine copy, no verification changes.

**Reason for import**: directly included by `scheduleredfunblock.c` and by
`scheduleruniimpl.h`; provides scheduler helper APIs used by the uniprocessor
scheduler path.

### scheduleruniimpl.h

**Source**: `cpukit/include/rtems/score/scheduleruniimpl.h`

**Status**: pristine copy, no verification changes.

**Reason for import**: contains `_Scheduler_uniprocessor_Unblock()` and heir
update helpers called by `_Scheduler_EDF_Unblock()`.

**Expected verification changes**: add contract boundaries for uniprocessor
heir-update behavior so EDF proofs do not have to inline all scheduler
mechanics.

### thread.h

**Source**: `cpukit/include/rtems/score/thread.h`

**Status**: pristine copy, no verification changes.

**Reason for import**: directly included by `scheduleredfunblock.c` and needed
for `Thread_Control` layout used by the unblock contract.

### scheduleredfunblock.c

**Source**: `cpukit/score/src/scheduleredfunblock.c`

**Status**: pristine copy, no verification changes.

**Reason for import**: first active EDF verification target for the new 6.2
port.

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

**Current scope**: contents only. This model intentionally does not describe
RBTree shape, colors, rotations, or traversal.

**Expected next changes**: add contracts for `_Scheduler_EDF_Enqueue()` in
terms of `edf_ready_set{Pre}` and `edf_ready_set{Here}`.

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
