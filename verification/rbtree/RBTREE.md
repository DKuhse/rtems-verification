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

Replaced `wf_node` with `bst_node(n, d, lo, hi)` that carries the BST
ordering property (every key in the subtree strictly in `(lo, hi)`) as
well as wellformedness. Added an uninterpreted logic function `key(n)`.

**Total: 53/53 goals Valid.** Three lemmas (`bst_node_valid`,
`bst_node_left`, `bst_node_key_in_bounds`) were all proved by
Alt-Ergo. No Coq escalation needed.

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

## What's left for probe 4

- **Global minimum claim.** "`key(\result) <= key(n)` for every `n` in
  the tree" requires quantifying over a set of nodes — an inductive
  `in_subtree{L}(root, n)` or a `\set<RBTree_Node*>`-valued
  `nodes_of(root)`. Not done here.
- **Tree construction.** `bst_node(root, g_tree_depth, g_lo, g_hi)` is
  still a precondition — nothing builds a tree that satisfies it. A
  probe targeting `_RBTree_Insert_with_parent` would need to prove
  insertion preserves `bst_node` (depth bound included).
- **RB-balance / black-height.** Orthogonal to BST ordering; would
  extend `bst_node` with a color/black-height component.
