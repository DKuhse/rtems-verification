# Deductive Verification for Earliest Deadline First Scheduler Implementation - Artifact Evaluation

This repository contains the code used to reproduce the evaluation results from our submission

_Deductive Verification for Earliest Deadline First Scheduler Implementation_

to RTSS 2026. The code in this repository reproduces the results from Section VII and contains the contracts used for the verification of the EDF schedulers in RTEMS 5, RTEMS 6, and FreeRTOS.
The artifact was tested on Ubuntu 24.04 LTS with 64 GB of RAM and an AMD Ryzen 9 3900X CPU and a Debian 13.4 x86 host with 8 GB of RAM and an Intel Core i3-10305T CPU.
On different platforms, the time measurements will naturally differ but show similar trends.

## Setup of the artifact environment
To run this artifact, several setup steps are needed. These are described below.

### Prerequisites
The artifact runs all benchmarks in a docker environment. Therefore, a docker installation is required. Other than that, a python3 interpreter is needed. We tested the scripts with Python 3.13.5.

### Build and source setup
The steps needed to set up the source code and verification environment are stated below:

1) Build the docker image:
```bash
docker compose build toolchain verify-6.2-active-fc32
```

2) Download the RTEMS source code (the source code for our FreeRTOS port is already included in this repository):
```bash
bash setup.sh
```

## Experiments
To execute the benchmarks that are used to generate the tables in the paper, please run this command:

```bash
python3 scripts/bench/bench.py run --pass parallel
```

During the execution, you will see an output like:
```
[bench  24/ 97] [#####...................]  24%  [6.2] h_thread_get_priority                     ok    10/10 goals   3.80s  (run 4m58s)
```
The statement `ok` indicates that this test was successful. In case a test fails, usually because of a timeout, the line will contain a `FAIL`. If a timeout occurs, consider increasing the time by setting the `WP_TIMEOUT` environment variable to a higher value. By default, it is set to 120 seconds per proof goal. 
We note that the goal count is for a single function target, while the table groups multiple targets together.

If all tests have finished successfully, the tables should be output in the terminal. Alternatively, please use this command to generate the LaTeX tables directly from the results:

```bash
python3 scripts/bench/bench.py render --parallel scripts/bench/results/results-parallel.txt
```

The full Frama-C logs of the benchmark runs are saved under `scripts/bench/results`.


### Sequential Execution of Experiments
Alternatively, we also include a script that uses only a single thread. For this, use this command to run the benchmarks:

```bash
python3 scripts/bench/bench.py run --pass serial
```

To generate the LaTeX table, after the benchmark script has run, please use this command:

```bash
python3 scripts/bench/bench.py render --serial scripts/bench/results/results-serial.txt
```

## Structure
The folder `scripts` contains individual verification scripts that are used to verify individual functions or files one at a time. We note that their goals do not line up with the table groups.

The folder `verification` contains the source code annotated with the proofs. For RTEMS this is an overlay. For FreeRTOS the relevant functions were separated into `reference` for readability.

For each verification project of RTEMS 5, RTEMS 6, and FreeRTOS, there is a model folder that includes the predicates and lemmas.
It is helpful to first read the model and subsequently read the contracts.
Specifically, edf_ready_set.h, edf_property.h and priority_aggregation.h for RTEMS
and 
list_model.h and scheduler_model.h for FreeRTOS.

### RTEMS contracts
For RTEMS, the most readable contracts are the contracts for the entry points of:

- `_Scheduler_EDF_Schedule`
- `_Scheduler_EDF_Unblock`
- `_Scheduler_EDF_Yield`
- `_Scheduler_EDF_Block`
- `_Scheduler_EDF_Update_priority` (scheduleredfchangepriority.c)

The contracts for

- `_Scheduler_EDF_Release_job`
- `_Scheduler_EDF_Cancel_job`

are more advanced since they interface with `_Rate_monotonic_Release_job` and `_Rate_monotonic_Cancel`. 
These contracts contain many preservation postconditions.

### FreeRTOS contracts
For FreeRTOS, the contract structure is less complicated.
Please first take a look at the model description and subsequently read the contracts.