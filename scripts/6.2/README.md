# Active RTEMS 6.2 Verification Scripts

This directory contains scripts for the new RTEMS 6.2 verification effort.

The previous hand-port scripts live under:

- `../../legacy/rtems-6.2-hand-port/scripts/6.2/`

New scripts should target the active overlay in `verification/6.2/` and should
not silently depend on the legacy stubs.

---

## ⚠️ Toolchain workaround: `vset.mlw` symlink

> **Read this before adding any script that proves a function whose contracts
> use `set<T>` / set comprehensions (anything that pulls in `edf_ready_set.h`
> or the `_Scheduler_EDF_Enqueue` contract).**

**Symptom.** Why3 fails every goal with
`[Why3 Error] Library file not found: vset (Stronger, N warnings)`.

**Root cause.** Packaging bug in our pinned stack
(Frama-C 25.0 + Why3 1.5.1, both from opam switch `4.14.1`).
`wp.driver` declares the `vset` library with

```
library vset:
type set = "set";
why3.import := "vset.Vset";
```

while every *other* WP library in the same file is namespaced as
`frama_c_wp.<name>.<Module>` (see `vlist`, `memory`, etc.). Why3 therefore
looks for `vset.Vset` at the loadpath root, but the actual file is
`<wp-share>/why3/frama_c_wp/vset.mlw`.

**Do NOT fix this by patching `wp.driver`.** Rewriting the import path to
`frama_c_wp.vset.Vset` makes Why3 load `memory.Memory` twice through two
different module paths, which then fails every goal with
`memory.Memory.addr and frama_c_wp.memory.Memory.addr` clash errors. (Asked,
answered, scars to show for it.)

**The workaround we use** is to expose `vset.mlw` at the unprefixed path the
driver actually asks for, via an idempotent symlink created by the verify
scripts at startup:

```sh
WP_WHY3_DIR="$(dirname "$(command -v frama-c)")/../share/frama-c/wp/why3"
if [ -f "${WP_WHY3_DIR}/frama_c_wp/vset.mlw" ] \
   && [ ! -e "${WP_WHY3_DIR}/vset.mlw" ]; then
    ln -s frama_c_wp/vset.mlw "${WP_WHY3_DIR}/vset.mlw"
fi
```

This is present in `verify-edf-unblock.sh`. **Any new script that triggers
the `vset` dependency must include the same shim**, otherwise it will fail
in a way that looks like a contract error but isn't.

**Why we don't just upgrade Frama-C.** This codebase is pinned to
Frama-C 25 / Why3 1.5.1 / Alt-Ergo 2.4.2 because the published verification
results (legacy 74/74, current active port) were obtained on that stack;
bumping Frama-C drags the prover stack and quietly shifts ACSL/WP behavior.
The right time to upgrade is between milestones, not while iterating on a
contract.

---

## Active Scripts

- `verify-edf-unblock.sh` — runs Frama-C on the active
  `_Scheduler_EDF_Unblock()` slice with `__FRAAMC__` defined so
  `scheduleredf.h` includes `verification/6.2/models/edf_ready_set.h`.
- `verify-scheduleruni-unblock.sh` — runs Frama-C/WP on the header harness for
  `_Scheduler_uniprocessor_Update_heir_if_necessary()`,
  `_Scheduler_uniprocessor_Update_heir_if_preemptible()`, and
  `_Scheduler_uniprocessor_Unblock()`, using the contract of
  `_Scheduler_uniprocessor_Update_heir()` as the CPU-state boundary. Expected
  result with `-wp-model 'Typed+Cast' -wp-timeout 30`: all goals proved.
- `tmp-verify-thread-get-priority.sh` — temporary isolation script for
  `_Thread_Get_priority()` and its immediate priority/home-node helper
  contracts.
