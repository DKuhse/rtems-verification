# EDF Scheduler Verification

## Execution

Build the image:

```bash
docker compose build toolchain verify-6.2-active-fc32
```

Obtain RTEMS source code (FreeRTOS is included in this repo)

```bash
bash setup.sh
```

To generate the tables for the benchmarks, execute the following commands:

```bash
python3 scripts/bench/bench.py run --pass serial # run serial benchmarks
python3 scripts/bench/bench.py run --pass parallel # run parallel benchmarks
python3 scripts/bench/bench.py render \
    --parallel scripts/bench/results/results-parallel.txt \ # print table for parallel execution
    --serial   scripts/bench/results/results-serial.txt # print table for serial execution
```

Full Frama-C logs are in bench/results.

If something times out on your machine, consider upping the timeouts, potentially drastically.


Verification scripts used in development:
```bash
docker compose run --rm verify-5.1-fc32 /opt/scripts/5.1/verify-all.sh
docker compose run --rm verify-6.2-active-fc32 /opt/scripts/6.2/verify-all.sh
docker compose run --rm verify-freertos /opt/scripts/freertos/verify-wp-all.sh
```
These don't suppress the Frama-C output, so it is very noisy, but at the end a summary table is printed.

Further scripts are in scripts/verify, covering individual functions/files.

## Structure

scripts/ has scripts to verify individual functions/files.
verification/ includes the source code with proofs. For RTEMS this is an overlay. For FreeRTOS the relevant functions were isolated into references/.

In each verification project, there is a model folder, that includes the predicates and lemmas.
For RTEMS, my recommendation is to start with the model and then look at the contracts of the entry points edf unblock, edf yield, edf block and edf change priority. edf release and cancel are particularly nasty because they interface with RM release/cancel, so lots of preservation postconditions were needed because the assigns did not suffice.

Note: 
Contracts are at times extremely verbose, as we state all preservations explicitly instead of trying to derive them from the assigns at the caller (which did not work well). 
There's also lots of debug proof-cuts ('asserts') left in. Ideally the unnecessary ones would be removed, but they are harmless and finding out which ones are unnecessary is quite tedious.