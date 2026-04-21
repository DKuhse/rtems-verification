# RBTree Verification — Side Experiment

Scope: a minimal probe to gauge how hard it is to verify the RTEMS 6.2
red-black tree with Frama-C/WP. Targets two functions:

- `_RBTree_Is_empty` — static inline, one-line `root == NULL` check.
- `_RBTree_Minimum` — non-inline, walks `rbe_left` pointers from root.

The goal is **difficulty assessment**, not a full verification.

## Relationship to the EDF verification

This experiment reuses the EDF setup's Docker image
(`rtems-edf-toolchain`), the 5.1 cross-compiler preprocessor
(`x86_64-rtems5-gcc -E`), and the already-extracted pristine 6.2 source
tree at `rtems/src/rtems-6.2-pristine/`. **It does not touch the EDF
verification files.**

Annotated headers in `include/rtems/score/` shadow the pristine headers
via Frama-C's `-I` order — the pristine tree stays read-only.

## Assumptions (unverified axioms)

These are trusted, not mechanically proved. They follow the same
trust-model as the EDF work's `g_min_*` ghost variables.

- **`_RBTree_Minimum` is modelled via a ghost `g_rbtree_min`.** The
  contract says `\result == g_rbtree_min`. The ghost is not constrained
  to actually be the BST minimum. A second pass would replace this with
  an inductive predicate over `rbe_left` pointers.

- **`RTEMS_RB_ROOT(tree) == tree->rbh_root`** — the macro expansion is
  trusted. ACSL contracts reference `the_rbtree->rbh_root` directly.

## Files

- `stubs-rbtree.h` — ghost declarations, pulled in via `-include`.
- `include/rtems/score/rbtree.h` — annotated copy of the pristine
  header; adds ACSL contracts on `_RBTree_Is_empty` and the
  `_RBTree_Minimum` prototype.
- `rbtreemin.c` — annotated copy of
  `cpukit/score/src/rbtreemin.c` with a loop invariant on the
  `rbe_left` walk.

## Running the experiment

From the repo root:

```
docker compose run --rm verify-rbtree verify-rbtree-is-empty.sh
docker compose run --rm verify-rbtree verify-rbtree-min.sh

# Interactive:
xhost +local:docker
docker compose run --rm gui /opt/scripts/rbtree/verify-rbtree-min.sh --gui
```

The `rbtree/` folder and `scripts/rbtree/` folder are mounted at
`/workspace/verification/rbtree` and `/opt/scripts/rbtree/` respectively
by the `verify-rbtree` compose service.

## Known pitfalls (carry-over risks for later scope)

- `rbtreemin.c` is thin here because the traversal is open-coded, but
  the insert/extract files are `RTEMS_RB_GENERATE_*` macro expansions
  — WP sees ~100 lines of generated recursive-style code per file.
  Use `frama-c -print` before writing contracts for those.
- Color is a **separate `int rbe_color`** in RTEMS 6.x `bsd-tree.h`
  (not bit-packed into the parent pointer). This avoids the `uintptr_t`
  bit-tagging pitfall in other BSD `sys/tree.h` variants.
- `_RBTree_Is_node_off_tree` sentinel uses `rbe_color == -1`; any
  future insert/extract scope needs that as a disjunct in contracts
  that touch off-tree nodes.

## Success criterion

A concrete answer — with goal counts from the `-wp` run — to:
"did `_RBTree_Minimum` close with Alt-Ergo, and if not, what blocked it?"
That finding, not a green proof, is the deliverable.

## Probe 1 result (vacuous proof)

Both targets closed 100% with only Qed.

- `_RBTree_Is_empty`: 2/2.
- `_RBTree_Minimum`:  22/22.

The proof was **structurally vacuous**: `_RBTree_Minimum` just returned
a ghost `g_rbtree_min`, and accessor contracts carried a trusted axiom
`\result != \null ==> \valid_read(\result)`. No tree invariant; no
termination claim. Useful as a smoke test of the toolchain.

## Probe 2 result (wellformedness-grounded)

Replaced the validity axiom on accessors with a derived consequence of
a real inductive predicate.

- `wf_node(n, d)` — depth-bounded inductive (null base case +
  step case requiring `\valid_read(n)` and wellformed children).
- Lemmas `wf_node_valid`, `wf_node_left` — **both proved by Alt-Ergo**
  (not trusted).
- `_RBTree_Minimum` takes `requires wf_node(tree->rbh_root, g_tree_depth)`
  on entry and carries `wf_node(node, d)` as the main loop invariant,
  with a ghost `d` that decrements on each iteration.
- `loop variant d` is added — termination of the left-walk is now
  proved.

**Total: 42/42 goals Valid.** Breakdown:
- 2 lemma goals (Alt-Ergo).
- 2 goals for `_RBTree_Is_empty`.
- 2 goals each for `_RBTree_Root` and `_RBTree_Left`.
- 34 goals for `_RBTree_Minimum` (5 needed Alt-Ergo — the wf_node
  preservation across the pointer chase; the longest run was 343ms).

### What became derived vs. trusted

| Property | Probe 1 | Probe 2 |
|---|---|---|
| `\valid_read` on left-chain nodes | axiom on accessors | **derived** from `wf_node_valid` |
| `wf_node_valid`, `wf_node_left` lemmas | n/a | **proved** by Alt-Ergo |
| Termination of the left-walk | not claimed | **proved** (loop variant) |
| BST order / minimum-is-the-minimum | not claimed | not claimed (future probe) |
| `RTEMS_RB_LEFT` / `RTEMS_RB_ROOT` macro semantics | trusted | trusted (unchanged) |

## Probe 3 result (BST invariant + leftmost claim)

Added an uninterpreted logic function `key(n)` and a BST ordering
predicate. The invariants are **layered** rather than monolithic:

- `wf_node(n, d)` — pure shape / wellformedness (unchanged from probe 2).
- `bst_order(n, lo, hi)` — pure BST ordering; every key in the subtree
  strictly in `(lo, hi)`. Independent of shape.
- `bst_node(n, d, lo, hi) := wf_node(n, d) && bst_order(n, lo, hi)` —
  composed predicate for contracts that need both. Functions that only
  need shape can require `wf_node` alone.

**Total: 55/55 goals Valid.** Five supporting lemmas, all proved by
Alt-Ergo:

- `wf_node_valid` — non-null wellformed ⇒ valid_read.
- `wf_node_left` — left child of wellformed is wellformed (lesser depth).
- `wf_node_depth_positive` — non-null wellformed ⇒ depth > 0. Needed
  because with the layered definition Alt-Ergo has to unfold
  `bst_node` and invert `wf_node` to discharge `d >= 0` preservation.
- `bst_order_key_in_bounds` — non-null ordered ⇒ `lo < key(n) < hi`.
- `bst_order_left` — left child is ordered with tightened `hi` bound.

No Coq escalation needed.

Strengthened ensures for `_RBTree_Minimum` (all proved):

- `\result->Node.rbe_left == \null` — the returned node is structurally
  leftmost.
- `key(\result) < g_hi` — the result's key is strictly below the tree's
  upper bound.
- `key(\result) <= key(tree->rbh_root)` — the result's key is at most
  the root's key (equal when the root itself is returned, strict
  otherwise).

The dynamic upper bound on the descending walk is expressed implicitly
through `parent` rather than via a tracked ghost `hi`. The invariant
splits on `parent == \null` and uses `key(parent)` as the tightening
bound once the first step has been taken. This dodged a ghost-code
restriction (logic functions can't be called from `ghost int x = ...`
assignments).

## Probe 4 result (global-minimum claim, fully discharged)

Added an inductive membership predicate `in_subtree(root, n)` and the
quantified ensures:

```c
ensures \result != \null ==>
  \forall RBTree_Node *n;
    in_subtree(tree->rbh_root, n) ==> key(\result) <= key(n);
```

This is the true global-minimum property: the returned node's key is
≤ every key in the tree.

**Total: 63/63 goals Valid.** Proof breakdown by prover:

- **42** via Qed (the built-in simplifier).
- **19** via Alt-Ergo 2.4.2.
- **2** via **Coq 8.13.2** — the two structural-induction lemmas that
  Alt-Ergo cannot handle.

### Coq infrastructure

`Dockerfile.rbtree` (at the repo root) extends `rtems-edf-toolchain`
with Coq 8.13.2 and Why3's Coq drivers. It produces the
`rtems-rbtree-toolchain` image used by the `verify-rbtree` compose
service. The `verify-rbtree-min.sh` script passes
`-wp-prover alt-ergo,coq` and `-wp-session
/workspace/verification/rbtree/wp-coq` so WP falls back to Coq on
lemmas Alt-Ergo can't close.

Persistent Coq scripts live at
`verification/rbtree/wp-coq/interactive/`. Two files:

- `lemma_in_subtree_descend_left.v` — ~7 lines of Ltac, induction on
  `P_in_subtree` with constructor reapplication at each case.
- `lemma_bst_order_bounds_subtree.v` — ~15 lines of Ltac, induction on
  `P_in_subtree` with `Q_bst_order_left` / `Q_bst_order_right` /
  `Q_bst_order_key_in_bounds` axiom applications and `Z.lt_trans`
  chaining. Needed a `revert`-and-reintroduce to generalize the
  bounds across the induction.

Both proofs compile cleanly with `coqc -Q /root/.opam/4.14.1/lib/why3/coq Why3 <file.v>`.

### Lemmas proved

- `in_subtree_root_non_null` — base case per constructor (Alt-Ergo).
- `in_subtree_split` — single-step case analysis (Alt-Ergo).
- `in_subtree_descend_left` — structural induction (**Coq**).
- `bst_order_bounds_subtree` — double induction (**Coq**).
- Three BST-ordering lemmas (`bst_order_left`, `bst_order_right`,
  `bst_order_key_in_bounds`) and three wellformedness lemmas
  (`wf_node_valid`, `wf_node_left`, `wf_node_depth_positive`), all
  Alt-Ergo.

## Probe 5a result (`_RBTree_Maximum` — infrastructure regression)

Added `_RBTree_Maximum` as a direct mirror of `_RBTree_Minimum`:
walks `rbe_right`, tightens `lo` bound instead of `hi` on each step,
symmetric ensures about global maximum.

**Total: 66/66 goals Valid.** Minimum also at 66/66 (the additions
increased the lemma count it can pull in). Proof breakdown per run:

- ~44 via Qed.
- ~19 via Alt-Ergo.
- **3** via Coq:
  - `bst_order_bounds_subtree` (probe 4)
  - `in_subtree_descend_left` (probe 4)
  - `in_subtree_descend_right` (probe 5a) — direct mirror of
    descend_left, ~7 lines of Ltac.

### What worked

Every predicate, ghost, and contract generalized. Only new
artifacts: `_RBTree_Right` accessor contract, `g_rbtree_max` ghost,
one symmetric lemma, and a new `rbtreemax.c` / `verify-rbtree-max.sh`.
The infrastructure from probes 2–4 holds.

### Small snag: Alt-Ergo 10s timeout

The quantified-invariant preservation for Maximum needed ~2.9s of
Alt-Ergo (vs ~1s for the Minimum equivalent). With the default 10s
WP timeout this closed fine first try; the WP session cache hit
from an earlier failed run kept reporting Timeout even after
`in_subtree_descend_right` was proved. Fix:
- Bumped `-wp-timeout 30` in both verify scripts.
- If you suspect a stale cache, delete `wp-coq/cache/` and re-run.

## Probe 5b result (`parent_linked` layer)

Added a third independent structural layer:

```c
inductive parent_linked{L}(RBTree_Node *n, RBTree_Node *expected_parent);
```

- `parent_linked(\null, _)` holds trivially.
- `parent_linked(n, ep)` holds iff `n->rbe_parent == ep` and both
  children are `parent_linked(_, n)`.
- For a whole tree: `parent_linked(tree->rbh_root, \null)`.

Three supporting lemmas, all proved by Alt-Ergo (no Coq needed):

- `parent_linked_parent_matches` — non-null member's parent pointer
  equals the expected parent.
- `parent_linked_left`, `parent_linked_right` — descending preserves
  linkage with the node itself as the new expected parent.

**Regression: Minimum and Maximum both at 69/69** (up from 66/66;
the three new lemma goals). Proof breakdown per run: 44 Qed, 22
Alt-Ergo, 3 Coq (unchanged — parent_linked didn't need Coq for
these easy lemmas).

### Why this is layered separately, not folded into `wf_node`

Keeps shape, ordering, and parent-linkage independent. Functions
that only walk via children (Minimum, Maximum) require only
`bst_node = wf_node && bst_order` — they don't need parent
linkage at all. Functions that walk via parents (Successor,
Insertion's color-fixup) will require the full
`bst_node && parent_linked(root, \null)` composition. This is a
cheap preparation.

### Not yet proved, needed for Successor

- `parent_linked_member_has_valid_parent` — for any non-root member
  of a parent-linked tree, `rbe_parent` is `\valid_read`. Needs
  structural induction on in_subtree — likely Coq.
- Termination of the walk-up loop — needs a ghost measure like
  "depth of node from root" or similar decreasing quantity.

## Probe 6a result (`_RBTree_Successor` partial correctness)

Annotated `_RBTree_Successor` via a hand-written `rbtreenext.c`
(bypassing the `RTEMS_RB_NEXT` macro expansion) — the body embeds
the standard successor algorithm with both branches (has-right-
subtree → leftmost-of-right, and no-right-subtree → walk up until
we came from a left child).

**Result: 65/65 goals Valid** for Successor.
**Regression: 72/72** on both Minimum and Maximum (up from 69 after
earlier additions). Proof breakdown per Minimum/Maximum run: 44 Qed,
23 Alt-Ergo, 5 Coq.

Two new Coq lemmas added this probe (total Coq lemmas now **5**):

- `wf_node_subtree_member_valid` — every member of a wellformed
  tree is `\valid_read`. ~10 lines of Ltac.
- `parent_linked_member_parent_in_tree` — for any non-root member
  of a parent-linked tree, `rbe_parent` points to another tree
  member. ~25 lines of Ltac with case-split via
  `why_decidable_eq` (Why3's `WhyType` decidable equality, cleaner
  than needing `Classical_Prop`).

Other added pieces:

- `g_root` ghost (the root of the tree containing `node`).
- `wf_node_right` lemma (symmetric of `wf_node_left`) — Alt-Ergo.

### Successor ensures proved

```c
ensures \result == \null || \valid_read(\result);
ensures \result != \null ==> in_subtree(g_root, \result);
```

Partial correctness only — enough to show the return value is a
legal tree pointer. Deferred claims:

- `\result != \null ==> key(\result) > key(node)` — requires
  propagating bst_order into node's right subtree and through the
  walk-up case. Needs `bst_order_subtree_member_ordered`-style
  lemma (dropped earlier as unnecessary for the minimal probe; it
  was also a Coq-territory structural induction).
- `\result == \null ==> node is the tree maximum` — requires
  reasoning about ancestors.
- Optimality (result is THE minimum key greater than node's) —
  needs set-level reasoning.

Termination still skipped (`-wp-prop="-@terminates"`).

### Coq workflow notes from this probe

- WP's `-wp-interactive=update` regenerates the stub from scratch,
  **including the header imports**. Additions like
  `Require Import Classical_Prop` get stripped. Use Why3's own
  primitives (`why_decidable_eq`) instead of classical reasoning.
- Use the host `Edit` tool on the `.v` files directly (they're
  now UID-1000 owned after the compose `user` mapping). No more
  Python-in-Docker heredocs.
- Fast iteration: `docker compose run --rm rbtree-shell
  /opt/scripts/rbtree/coq-check.sh <file.v>` for compile-only
  feedback; only invoke the full `verify-rbtree-next.sh` once the
  proof compiles clean.

## What's left for probe 6b / 7+

- **Successor full correctness** — add back `key(result) > key(node)`
  and the null-means-maximum / optimality claims. Needs
  `bst_order_subtree_member_ordered` (Coq) plus ancestor-chain
  reasoning for the walk-up case.
- **`_RBTree_Predecessor`** — mirror of Successor.
- **Tree construction.** `_RBTree_Insert_with_parent` preserving
  `bst_node && parent_linked`. The real frontier.
- **RB-balance / black-height.** Orthogonal to BST ordering.
