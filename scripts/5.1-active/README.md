# Active RTEMS 5.1 Verification Scripts

This directory contains scripts for the new RTEMS 5.1 overlay-style verification
effort. The 5.1 source tree at `rtems/src/rtems-5.1-pristine/` is left
untouched; the verification overlay at `verification/5.1/overlay/` is preferred
via `-I` ordering when Frama-C runs.

The legacy in-tree 5.1 verification scripts (`scripts/verify-*.sh`) and the
docker `verify` service still work against `rtems/src/rtems-5.1/` (which is
patched by `setup.sh`). They are independent of this effort.

## Active Scripts

All run against the FC 32 stack via `docker compose run --rm verify-5.1-active-fc32`.
The scripts also detect FC 25 and switch the C standard flag from `-std c11`
to `-c11` so quick compatibility checks can run on `verify-5.1-active`.

- `verify-edf-initialize.sh` — runs Frama-C/WP on the active
  `_Scheduler_EDF_Initialize()` slice and checks that initialization produces
  an empty, well-formed EDF ready context.

Additional scripts (block, schedule, unblock, yield, update_priority,
release_job, cancel_job, thread-change-priority, scheduler-release-job,
ratemon-release-job, ratemon-cancel) will be added as the corresponding source
file contracts are ported from `verification/6.2/` (see
`verification/5.1/HEADERS.md` for the per-file porting state).

## Toolchain Notes

The 5.1 active scripts share Stage 1 (RTEMS cross-toolchain image, AMD64 BSP)
with the existing `verify` service. The Frama-C stack is the same as the
active 6.2 port:

| stack | service | scripts | purpose |
|---|---|---|---|
| Frama-C 25.0 + Alt-Ergo 2.4.2 | `verify-5.1-active` | this directory | byte-reproducibility on legacy stack (limited) |
| Frama-C 32.0 + Alt-Ergo 2.6.x | `verify-5.1-active-fc32` | this directory | active-port work, EDF property proofs |

The same FC32 migration gotchas as 6.2 apply: `-std c11` flag rename,
abstract set-comprehension axiomatisation, forward-declare implicit-decl
timestamp helpers in slice files.

## What is verified now vs pending

See `verification/5.1/HEADERS.md` for the canonical per-file porting state.
