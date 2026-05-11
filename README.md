# EDF Scheduler Verification

scripts/ has scripts to verify individual functions.
verification/ includes the annotated source code that is overlayed on top of the original source code.
original source code needs to be obtained separately (see setup.sh).

## Active RTEMS 6.2 Direction

The previous RTEMS 6.2 verification hand-port has been quarantined under
`legacy/rtems-6.2-hand-port/`. It is kept as reference material, not as the
trusted active verification path.

The active RTEMS 6.2 plan is now:

- `verification/6.2/EDF_RBTREE_ABSTRACTION_PLAN.md`

The active RTEMS 6.2 scaffold is:

- `verification/6.2/overlay/`
- `verification/6.2/models/`
- `verification/6.2/headers/`
- `scripts/6.2/`

The next 6.2 implementation should build a clean abstract RBTree contract layer
instead of extending the legacy ghost-stub hand-port.

## Reproduce

```sh
docker compose build                    # Frama-C 25 + RSB cross-toolchain
./setup.sh                              # download/extract sources, build BSP, apply 5.1 patches
docker compose run --rm verify          # RTEMS 5.1 — runs verify-wp-all.sh
docker compose run --rm verify-6.2      # legacy RTEMS 6.2 hand-port
docker compose run --rm verify-6.2-active # active RTEMS 6.2 abstract RBTree port
docker compose run --rm verify-freertos # FreeRTOS EDF (MSP430) — smoke test only for now
```

Run the active 6.2 unblock slice:

```sh
docker compose run --rm verify-6.2-active
```

Run a single legacy 6.2 function:

```sh
docker compose run --rm verify-6.2 \
    /opt/scripts/6.2/verify-edf-update-priority.sh \
    -wp -wp-fct _Scheduler_EDF_Update_priority -wp-model 'Typed+Cast'
```

GUI (frama-c-gui via X11):

```sh
xhost +local:docker && docker compose run --rm gui
```


## Shell

Want to play around with the code and Frama-C? Get a shell in the container:

```sh
docker compose run --rm --entrypoint bash verify-6.2
docker compose run --rm --entrypoint bash verify-6.2-active
docker compose run --rm --entrypoint bash verify-freertos
```
