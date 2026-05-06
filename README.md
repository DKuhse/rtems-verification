# EDF Scheduler Verification

scripts/ has scripts to verify individual functions.
verification/ includes the annotated source code that is overlayed on top of the original source code.
original source code needs to be obtained separately (see setup.sh).


## Reproduce

```sh
docker compose build                    # Frama-C 25 + RSB cross-toolchain
./setup.sh                              # download/extract sources, build BSP, apply 5.1 patches
docker compose run --rm verify          # RTEMS 5.1 — runs verify-wp-all.sh
docker compose run --rm verify-6.2      # RTEMS 6.2 — runs verify-wp-all.sh -wp-timeout 30
docker compose run --rm verify-freertos # FreeRTOS EDF (MSP430) — smoke test only for now
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


## Shell

Want to play around with the code and Frama-C? Get a shell in the container:

```sh
docker compose run --rm --entrypoint bash verify-6.2
docker compose run --rm --entrypoint bash verify-freertos
```