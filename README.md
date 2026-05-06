# EDF Scheduler Verification

Formal verification of the RTEMS EDF scheduler with Frama-C/WP, ported
from RTEMS 5.1 to RTEMS 6.2.

Both versions verify at 100% (5.1: 3,963/3,963; 6.2: 4,804/4,804) using
the `Typed+Cast` memory model with Alt-Ergo 2.4.2.

Builds on the original
[Formally-Verifying-Implementations-of-EDF-Scheduler-in-RTEMS](Formally-Verifying-Implementations-of-EDF-Scheduler-in-RTEMS/)
work.

## Reproduce

```sh
docker compose build               # Frama-C 25 + RSB cross-toolchain
./setup.sh                         # download/extract sources, patch, copy overlay files
docker compose run --rm verify     # RTEMS 5.1 — runs verify-wp-all.sh
docker compose run --rm verify-6.2 # RTEMS 6.2 — runs verify-wp-all.sh -wp-timeout 30
```

Run a single function:

```sh
docker compose run --rm verify-6.2 \
    /opt/scripts/6.2/verify-edf-update-priority.sh \
    -wp -wp-fct _Scheduler_EDF_Update_priority -wp-model 'Typed+Cast'
```

GUI (frama-c-gui via X11):

```sh
xhost +local:docker && docker compose run --rm gui
```