# Legacy RTEMS 6.2 Verification Hand-Port

This directory contains the previous RTEMS 6.2 verification hand-port. It is
kept as reference material only.

The active RTEMS 6.2 effort is now documented in:

- `../../verification/6.2/EDF_RBTREE_ABSTRACTION_PLAN.md`

Do not treat the contracts and stubs here as the trusted active proof model.
They include hand-ported ACSL, ghost variables, and helper stubs inherited from
the RTEMS 5.1 verification approach. Use them to recover intent and compare
behavior while rebuilding the proof around an explicit abstract RBTree model.

Contents:

- `verification/6.2/` — legacy overlay, simplified headers, stubs, and notes
- `scripts/6.2/` — legacy Frama-C/WP invocation scripts
- `CHANGES.md`, `PORTING.md`, `TODO.md` — historical hand-port notes
