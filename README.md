# EDF Scheduler Verification

## Execution

RTEMS source code needs to be obtained separately by executing setup.sh. 
FreeRTOS source code is included.


docker compose run --rm verify-5.1-fc32 /opt/scripts/5.1/verify-all.sh
docker compose run --rm verify-6.2-active-fc32 /opt/scripts/6.2/verify-all.sh
docker compose run --rm verify-freertos /opt/scripts/freertos/verify-wp-all.sh


To generate the tables for the benchmarks, execute the following commands:

python3 scripts/bench/bench.py run --pass serial # run serial benchmarks
python3 scripts/bench/bench.py run --pass parallel # run parallel benchmarks
python3 scripts/bench/bench.py render \
    --parallel scripts/bench/results/results-parallel.txt \ # print table for parallel execution
    --serial   scripts/bench/results/results-serial.txt # print table for serial execution



If something times out on your machine, consider upping the timeouts:



## Structure

scripts/ has scripts to verify individual functions/files.
verification/ includes the source code with proofs. For RTEMS this is an overlay. For FreeRTOS the relevant functions were isolated into references/.
