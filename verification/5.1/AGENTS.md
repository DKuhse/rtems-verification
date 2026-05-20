This is a verification of UP EDF scheduling in RTEMS 5.1. Focus lies on EDF API functions.
RBTree operations are out of scope. The wrapper functions for it are given contracts and not verified.
SMP is out of scope.

Use docker compose to run the verification. This is a beefy machine. If a timeout of 30 seconds is not sufficient, it's very unlikely a higher timeout will help.
Do not modify the actual C code.

This is a back-port of the active 6.2 verification effort (`verification/6.2/`). The modeling approach,
abstract ready-set, EDF property, and priority aggregation models transfer 1:1. The 5.1 EDF entry-point
code paths differ from 6.2 — in particular, 5.1 has no `scheduleruniimpl.h` and inlines heir update through
`_Scheduler_Generic_block()` and `_Scheduler_EDF_Schedule_body()`. See `PORT_NOTES.md` for the version diff.
