# Changes to Verification Annotations

Changes made to the ACSL annotations and WP invocation from the original
[Formally-Verifying-Implementations-of-EDF-Scheduler-in-RTEMS](Formally-Verifying-Implementations-of-EDF-Scheduler-in-RTEMS/)
repository that brought all proof goals to 100%.

## Unverified Assumptions (5.1 and 6.2)

The following assumptions are trusted axioms in the verification — they
are not mechanically proved by Frama-C/WP. They are inherited from the
original 5.1 verification approach and apply equally to the 6.2 port.

### Function pointer resolution (`//@ calls`)

The priority aggregation functions (`_Priority_Non_empty_insert`,
`_Priority_Extract_non_empty`, `_Priority_Changed`) call a change
handler through a function pointer: `( *change )( ... )`. WP cannot
resolve indirect calls — it does not reason about function pointer
values. The `//@ calls _Thread_Priority_action_change` annotation
tells WP which function's contract to apply. This is a trusted axiom.

The contracts also include
`requires change == _Thread_Priority_action_change`, which WP does
verify at each call site. However, WP does not connect this proved
fact to the `//@ calls` annotation — the two are independent. This is
a limitation of Frama-C 25's WP plugin.

### Red-black tree operations (ghost variables)

The red-black tree functions (`_RBTree_Minimum`, `_RBTree_Insert_inline`,
`_RBTree_Extract`) are abstracted via ghost variables
(`g_min_priority_node`, `g_new_minimum`, `g_min_edf_node`). The
verification assumes these ghost variables correctly model the tree's
behavior — that the minimum is correctly tracked and that insertions
correctly report new minimums. The tree operations themselves are not
verified.

### `SCHEDULER_NODE_OF_WAIT_PRIORITY_NODE` macro

The `RTEMS_CONTAINER_OF` macro used in
`SCHEDULER_NODE_OF_WAIT_PRIORITY_NODE` is replaced with a stub function
`_Helper_SCHEDULER_NODE_OF_WAIT_PRIORITY_NODE` that returns a ghost
variable. The assumption is that the macro correctly computes the
containing struct pointer.

---

## 1. WP memory model: Typed+Cast

Pass `-wp-model "Typed+Cast"` to frama-c. The default `Typed` model cannot
reason through `Scheduler_Node*` to `Scheduler_EDF_Node*` casts used
throughout the ACSL contracts.

## 2. Added `\separated` clauses vs `_Per_CPU_Information`

When `_Scheduler_Update_heir` is inlined, it accesses the global
`_Per_CPU_Information` array. WP needs to know function parameters don't
alias with it.

**scheduleredfchangepriority.c** — added to `_Scheduler_EDF_Update_priority` contract:
```c
requires \separated(node + (..), (Per_CPU_Control_envelope *)_Per_CPU_Information + (..));
requires \separated(scheduler + (..), (Per_CPU_Control_envelope *)_Per_CPU_Information + (..));
requires \separated(the_thread + (..), (Per_CPU_Control_envelope *)_Per_CPU_Information + (..));
```

**scheduleredfunblock.c** — added to `_Scheduler_EDF_Unblock` contract:
```c
requires \separated(node + (..), (Per_CPU_Control_envelope *)_Per_CPU_Information + (..));
requires \separated(the_thread + (..), (Per_CPU_Control_envelope *)_Per_CPU_Information + (..));
requires \separated(scheduler + (..), (Per_CPU_Control_envelope *)_Per_CPU_Information + (..));
```

## 3. Added `\from` clauses on pointer-returning functions

Changed `assigns \nothing` to `assigns \result \from ...` for functions
returning pointers, so WP can track data dependencies precisely:

- **stubs.h**: `_Helper_RBTree_EDF_Minimum` → `assigns \result \from tree, g_min_edf_node;`
- **stubs.h**: `_Thread_Get_CPU` → `assigns \result \from thread;`
- **scheduleredfimpl.h**: `_Scheduler_EDF_Get_context` → `assigns \result \from scheduler, g_edf_sched_context;`
- **scheduleredfimpl.h**: `_Scheduler_EDF_Node_downcast` → `assigns \result \from node;`
- **schedulerimpl.h**: `_Scheduler_Update_heir` new_h behavior → `assigns _Thread_Heir \from new_heir; assigns _Thread_Dispatch_necessary \from \nothing;`

## 4. Commented out unprovable `_Thread_Dispatch_necessary` ensures

**scheduleredfchangepriority.c** — commented out
`ensures _Thread_Dispatch_necessary == true;` in the `exec_update_new_h`
behavior of `_Scheduler_EDF_Update_priority`.

The `dispatch_necessary` field in `Per_CPU_Control` is declared
`volatile bool` (`percpu.h:400`). Frama-C/WP correctly refuses to prove
postconditions about volatile writes — the C standard says volatile
variables may change at any time by external means (here: interrupt
context), so a write cannot be assumed to persist at function exit.

The code does set `_Thread_Dispatch_necessary = true`
(`schedulerimpl.h:1225`), but this is fundamentally unprovable under
volatile semantics. The inner contracts (`_Scheduler_Update_heir` and
`_Scheduler_EDF_Schedule_body`) already had this ensures commented out.
The `_Scheduler_EDF_Unblock` contract had it commented out for the same
reason.

The field is volatile because it is read from interrupt handlers and
written from the scheduler — `volatile` prevents the compiler from
optimizing away the write. But `volatile` also means "may change at any
time without the program doing anything," so WP correctly refuses to
assume the write persists until the postcondition. Writing `true` to a
volatile variable does not let you prove it is still `true` one
instruction later — that is the semantics of `volatile` in C.

There is no way to make this provable without removing `volatile`, which
would be incorrect for the actual RTEMS code.

# RTEMS 6.2 Port — Key Findings

The verification was ported from RTEMS 5.1 to 6.2. The contracts are
a mechanical adaptation (bool → Priority_Group_order enum), but four
non-obvious issues required debugging:

## 1. Flexible arrays in `thread.h`

RTEMS 6.2 has two `RTEMS_ZERO_LENGTH_ARRAY` members in `thread.h`:
- Line 641: `Thread_queue_Heads Thread_queue_heads[]` (in `Thread_Proxy_control`)
- Line 1009: `void *extensions[]` (in `Thread_Control`)

WP cannot compute `\valid` for structs containing zero-length arrays
because the size is undefined. This caused 9 "Degenerated" goals —
WP simplified the proof obligations to `false`.

**Fix:** Comment out both lines (same approach as 5.1's line-883 patch).

## 2. `-wp-split` flag required

WP's proof partitioning produces different splits for 6.2 than 5.1.
Without `-wp-split`, WP generates 3 coarse `_partN` sub-goals per
postcondition (too complex for Qed/Alt-Ergo). With `-wp-split`, WP
generates 12+ fine-grained parts matching 5.1's structure.

**Root cause:** Different struct layouts in 6.2 after preprocessing
change how WP's internal simplifier decomposes the proof.

**Fix:** Pass `-wp-split` to frama-c for all verification targets.

## 3. `//@ calls` annotation for function pointer callbacks

The `_Priority_Non_empty_insert`, `_Priority_Extract_non_empty`, and
`_Priority_Changed` functions call a change handler through a function
pointer: `( *change )( aggregation, group_order, actions, arg )`. WP
cannot reason about function pointer calls without knowing which
function is being called.

In 5.1, `//@ calls _Thread_Priority_action_change` annotations were
in `priorityimpl.h`. In 6.2, these required:
- A forward declaration of `_Thread_Priority_action_change` inside
  `priorityimpl.h` (after the `Priority_Group_order` enum definition)
- The `//@ calls` annotation on each function pointer call site

Without this, ALL "new minimum" paths failed (101 of 915 goals in
`_Thread_Priority_do_perform_actions`). With it: 993/993.

**Soundness note:** `//@ calls` is an unverified axiom — WP trusts
that the function pointer resolves to the named function and uses
that function's contract. It does not derive this from the code.

The contracts also include
`requires change == _Thread_Priority_action_change`, which WP does
verify at each call site. This guards against the caller passing a
different function — but it does not mechanically link to the
`//@ calls` annotation. If `//@ calls` named a different function
than the `requires`, WP would not detect the inconsistency.

So this is an axiom with a verified guard: the `requires` prevents
silent breakage if call sites change, but the `//@ calls` itself
remains a trusted annotation. This is the same trust level as the
ghost variables (`g_min_priority_node`, etc.) — assertions about
runtime state that the framework assumes rather than proves.

## 4. Enum completeness precondition

C enums are integers — `Priority_Group_order` can theoretically hold
values other than `PRIORITY_GROUP_FIRST` (0) and `PRIORITY_GROUP_LAST`
(1). WP cannot prove `complete behaviors` for enum-based behavior
splits without knowing the enum is constrained.

In 5.1, `bool` gave trivial completeness (`x || !x`). In 6.2:

**Fix:** Add `requires priority_group_order == PRIORITY_GROUP_FIRST ||
priority_group_order == PRIORITY_GROUP_LAST` to every contract that
has enum-based `complete behaviors`.

## 5. Full inlining is better than standalone contracts for uniprocessor layer

The `scheduleruniimpl.h` functions (`_Scheduler_uniprocessor_Update_heir`,
`_Update_heir_if_preemptible`, etc.) were tested with standalone ACSL
contracts instead of inlining. This produced WORSE results — WP loses
`\separated` hypotheses at contract boundaries that it preserves
through inlined code. Full inlining (all 4 levels) gives the best
proof rate.

3 goals remain unprovable in `_Scheduler_EDF_Update_priority` (2) and
`_Scheduler_EDF_Unblock` (1). These are `assigns` goals about the
`_Thread_Heir` pointer assignment inside the inlined
`_Scheduler_uniprocessor_Update_heir`. The deeper call chain in 6.2
(4 levels vs 5.1's 2 levels) makes these goals harder. Same root
cause as the 5.1 `\from` / Per_CPU issue.

## 6. `CPU_STRUCTURE_ALIGNMENT` attribute position

RTEMS 6.2 moved the alignment attribute from the variable to the type:
```c
// 5.1: attribute on variable (doesn't change type)
extern Per_CPU_Control_envelope _Per_CPU_Information[1U] CPU_STRUCTURE_ALIGNMENT;

// 6.2: attribute on type (changes type for WP)
extern CPU_STRUCTURE_ALIGNMENT Per_CPU_Control_envelope _Per_CPU_Information[1U];
```

**Fix:** Move attribute back to variable position in patched `percpu.h`
so `\separated` clauses match WP's memory model hypotheses.
