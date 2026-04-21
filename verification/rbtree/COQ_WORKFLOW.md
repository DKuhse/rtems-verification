# Coq Proof Development — Workflow

The probe-4 lemmas `in_subtree_descend_left` and
`bst_order_bounds_subtree` are discharged via Coq because Alt-Ergo
2.4.2 cannot automate structural induction on ACSL inductive
predicates. The proof scripts live at
`verification/rbtree/wp-coq/interactive/*.v` and must be checked into
version control alongside the ACSL.

This file documents the iteration loop that worked.

## Setup (one time)

The `rtems-rbtree-toolchain` image includes Coq 8.13.2 and the Why3
Coq drivers. It runs as UID 1000 (via `user: "1000:1000"` in
`docker-compose.yml`), so files generated or edited inside the
container are owned by the host user.

If you also have Coq installed locally for VSCode's Coq extension,
drop a `_CoqProject` file in `wp-coq/interactive/`:

```
-R <your-local-why3-coq-lib> Why3
```

Find the path with `opam var lib` on the host (it'll be something
like `~/.opam/<switch>/lib/why3/coq`). Use `-R` not `-Q` because
the WP-generated files use bare `Require Import BuiltIn.` without
qualification. VSCode-Coq picks up this file automatically and
`Require Import` lines resolve.

## The iteration loop

**Don't spin up a fresh container per iteration.** Per-command
container startup is 2–3 seconds; for Coq work where you edit,
check, edit, check, that adds up fast. Use the persistent
`rbtree-shell` service:

```
docker compose run --rm rbtree-shell
```

From inside the container, your working directory is
`/workspace/verification/rbtree`. You have `frama-c`, `coqc`, and
the `coq-check.sh` helper on your path.

### Step 1 — generate the stub (once per lemma)

From the persistent shell:

```
frama-c \
  -cpp-command "..." \
  -machdep gcc_x86_64 -cpp-frama-c-compliant -c11 \
  -wp -wp-model 'Typed+Cast' -wp-split \
  -wp-prop='<lemma_name>' \
  -wp-prover coq \
  -wp-session /workspace/verification/rbtree/wp-coq \
  -wp-interactive=update \
  rbtreemin.c
```

This creates `wp-coq/interactive/lemma_<name>.v` with an empty
`Proof. ... Qed.` block. Do NOT re-run with `-wp-interactive=update`
after you've started editing — it may overwrite.

### Step 2 — write the proof, check with coqc

Edit the `.v` file in VSCode on the host (files are user-owned
thanks to the UID mapping). Use the Coq extension to step through
tactics interactively.

To validate from the container shell:

```
coq-check.sh wp-coq/interactive/lemma_<name>.v
```

Clean run = proof compiles. Errors include the current proof state,
which is much more informative than WP's "Unknown."

### Step 3 — let WP confirm the proof slot

Once `coq-check.sh` is clean, run the full verify script:

```
/opt/scripts/rbtree/verify-rbtree-min.sh
```

WP sees the saved proof in the session directory and reports the
lemma as "Valid (Coq)."

## Pitfalls we hit

- **WP's "Unknown" hides Coq errors.** When the proof doesn't
  type-check, WP just reports Unknown. Always run `coq-check.sh`
  first to see the real error.
- **WP regenerates stubs with `-wp-interactive=update`.** Your
  proof edits will be lost. Only regenerate when constructor names
  or types in the ACSL predicates change.
- **Induction hypothesis over-specializes unless you `revert`.**
  When the goal has free variables that appear in other hypotheses
  (e.g., `h2 : P_bst_order t a1 i1 i`), `induction h1` fixes
  `i, i1` before inducting, leaving an IH that can't be re-applied
  at other bounds. Fix: `revert i i1 h2.` before `induction h1`,
  then `intros` back inside each case.
- **Argument order from the ACSL doesn't match Coq's Theorem order.**
  ACSL `\forall root, n, lo, hi;` becomes
  `forall (t:addr->addr) (i i1:Z) (a a1:addr)` in the generated
  Coq — WP orders parameters by (map, integer, pointer), and
  `lo`/`hi` become `i1`/`i` (!). Read the stub carefully.
- **Why3 axioms are per-lemma-file.** Each generated `.v` file
  inlines copies of the axioms it needs (`Q_bst_order_left`,
  `Q_bst_order_key_in_bounds`, etc.) as Coq axioms. If you add a
  new lemma to ACSL and want to use it in a Coq proof, regenerate
  the stub — the axioms section will include the new one.

## Why this layout

- The `.v` files in `wp-coq/interactive/` are the source of truth
  for the Coq proofs. Commit them.
- The `rbtree-shell` compose service is the intended dev
  environment — not `verify-rbtree`, which is for CI-style one-shot
  checks.
- `coq-check.sh` is the tight loop for proof development;
  `verify-rbtree-min.sh` is the confirmation step.

## Future improvements

- Install `coq-ide` in the Dockerfile and wire it into the `gui`
  service for graphical proof stepping when VSCode isn't convenient.
- Cache compiled Why3 Coq libraries at a predictable host path
  (bind mount) so the VSCode-Coq extension resolves imports
  without needing a host Coq install.
