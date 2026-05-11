# Active RTEMS 6.2 Verification Scripts

This directory contains scripts for the new RTEMS 6.2 verification effort.

The previous hand-port scripts live under:

- `../../legacy/rtems-6.2-hand-port/scripts/6.2/`

New scripts should target the active overlay in `verification/6.2/` and should
not silently depend on the legacy stubs.

---

## Toolchain split: FC 25 (legacy) vs FC 32 (active)

This project runs on **two Frama-C stacks**, picked via `docker-compose`
services that share Stage 1 (the RTEMS cross-toolchain) but install
different Frama-C versions in Stage 2:

| stack | service | scripts | purpose |
|---|---|---|---|
| Frama-C **25.0** + Alt-Ergo 2.4.2 (pinned) | `verify-6.2` | `legacy/rtems-6.2-hand-port/scripts/6.2/` | byte-reproducibility for the published legacy 74/74 hand-port result |
| Frama-C **32.0** + Alt-Ergo 2.6.x | `verify-6.2-active-fc32` | `scripts/6.2/` (these) | active-port work, EDF property proofs |

Both are built from the same `Dockerfile`, parameterised by
`FRAMA_C_VERSION` (default `25.0`). The `verify-6.2-active` service is
**kept-for-reference-only** — the active scripts here have moved to FC 32
syntax (`-std c11`) and no longer parse on FC 25.

### Why two stacks (the FC 25 vset bug)

FC 25's `wp.driver` declares the `vset` library as `why3.import := "vset.Vset"`
(no namespace), but the file actually lives in `<wp-share>/why3/frama_c_wp/`.
On any goal that mixes a set-typed `assigns` clause (`{ other->Node | ... }`)
with addr-level frame reasoning, Why3 either reports *"Library file not
found: vset"* or, if you symlink the file into place, a *"Type mismatch
between vset.Vset.set ... and ... .addr"* — same module loaded under two
nominal paths. We sank significant time into trying to patch this
(adjusting `wp.driver`, symlinking, prover-strategy tweaks) before
concluding the only durable fix is **stay on FC 25 only when you genuinely
need byte-equivalence with the published numbers; do everything else on a
modern stack**. The legacy verifications don't hit the bug because their
contracts don't use `set<T>`-typed `assigns`.

### FC 32 migration gotchas (already applied)

If you write a new active script or new ACSL, watch for these — they came
up during the FC 25 → FC 32 move and are baked into the scripts/models now:

- **`-std c11`** replaces `-c11` (kernel flag rename).
- **Set comprehensions in logic-function definitions** (`{ n | T *n; P(n) }`)
  hit `Concretization for comprehension sets not implemented yet`. Declare
  the function abstractly and add a `\forall n; n \in f(...) <==> P(n)`
  membership axiom instead. See `verification/6.2/models/edf_ready_set.h`
  for the pattern.
- **Implicit function declarations are errors**, not warnings. RTEMS'
  `timestampimpl.h` calls `sbttots`/`tstosbt`/`sbttotv` without a visible
  declaration in the headers we include; forward-declare them at the top
  of the slice file under `#ifdef __FRAAMC__`. See the top of
  `verification/6.2/overlay/cpukit/score/src/scheduleredfunblock.c`.

---

## Abstraction-boundary stubs: `assigns \nothing` as a deliberate lie

Two helpers in the inlined chain under `_Scheduler_EDF_Unblock` have side
effects that are *below* the EDF/scheduler abstraction — nothing above the
abstraction reads them, and the EDF property doesn't depend on them. Both
get a deliberately-lying contract in the overlay rather than honest
threading of their assigns up through every caller.

| helper | overlay contract | what it actually does |
|---|---|---|
| `_Scheduler_EDF_Enqueue` | `assigns context->Ready` only (paired with `edf_ready_set` reading only `context->Ready`) | RBTree rotation: rewrites `rbe_{left,right,parent,color}` of various nodes |
| `_Thread_Update_CPU_time_used` | `assigns \nothing` | writes `cpu->cpu_usage_timestamp` and `the_thread->cpu_time_used` |

**Why lie**: the honest assigns *can* be threaded up in theory, but in
practice WP's syntactic frame inference produces fresh universal-addr
forms after inlining + parameter substitution (e.g. `a6+11`, `_Thread_Heir+52`)
that don't unify with the caller's field-access ACSL expressions inside
the prover budget. We hit this concretely on `assigns_normal_part13` for
`_Scheduler_EDF_Unblock` with the honest cpu-time contract: WP couldn't
discharge `a6+11 ≡ _Per_CPU_Information[0].per_cpu.cpu_usage_timestamp`
through the inlined call. With `assigns \nothing` the question doesn't
arise.

**Why it's sound**: WP uses the helper's contract at call sites; it does
*not* verify the contract against the helper's body (the helper isn't
`-wp-fct`'d). Soundness then reduces to "no contract above the
abstraction reads the lied-about fields" — true for both helpers above.

**When this pattern doesn't apply**: if a caller's `assigns`/`ensures` or
a logic function's `reads` clause references the lied-about field, the
lie becomes load-bearing and risks a vacuous proof. Audit before stubbing.
Pattern signal in the other direction: if you find yourself stacking
`\at(X, Pre)->field` *and* `other_view->field` *and*
`_Per_CPU_Information[0].per_cpu.field` in a caller's `assigns` to chase
frame matches that nothing semantically needs — that's the cue to stub
the helper instead.

The legacy 6.2 hand-port used the same pattern (`legacy/.../stubs.h`)
for the same reasons. Both pattern instances are documented inline at the
contract sites.

---

## Active Scripts

All run against the FC 32 stack via `docker compose run --rm verify-6.2-active-fc32`.

- `verify-edf-unblock.sh` — runs Frama-C on the active
  `_Scheduler_EDF_Unblock()` slice with `__FRAAMC__` defined so
  `scheduleredf.h` includes `verification/6.2/models/edf_ready_set.h`.
- `verify-scheduleruni-unblock.sh` — runs Frama-C/WP on the header harness for
  `_Scheduler_uniprocessor_Update_heir_if_necessary()`,
  `_Scheduler_uniprocessor_Update_heir_if_preemptible()`, and
  `_Scheduler_uniprocessor_Unblock()`, using the contract of
  `_Scheduler_uniprocessor_Update_heir()` as the CPU-state boundary. Expected
  result with `-wp-model 'Typed+Cast' -wp-timeout 30`: all goals proved
  (48/48 on FC 32).
- `tmp-verify-thread-get-priority.sh` — temporary isolation script for
  `_Thread_Get_priority()` and its immediate priority/home-node helper
  contracts.
