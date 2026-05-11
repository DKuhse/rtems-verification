# Active RTEMS 6.2 Verification Scripts

This directory contains scripts for the new RTEMS 6.2 verification effort.

The previous hand-port scripts live under:

- `../../legacy/rtems-6.2-hand-port/scripts/6.2/`

New scripts should target the active overlay in `verification/6.2/` and should
not silently depend on the legacy stubs.

## Active Scripts

- `verify-edf-unblock.sh` — runs Frama-C on the active
  `_Scheduler_EDF_Unblock()` slice with `__FRAAMC__` defined so
  `scheduleredf.h` includes `verification/6.2/models/edf_ready_set.h`.
- `verify-scheduleruni-unblock.sh` — runs Frama-C/WP on the header harness for
  `_Scheduler_uniprocessor_Update_heir_if_necessary()`,
  `_Scheduler_uniprocessor_Update_heir_if_preemptible()`, and
  `_Scheduler_uniprocessor_Unblock()`, using the contract of
  `_Scheduler_uniprocessor_Update_heir()` as the CPU-state boundary.
