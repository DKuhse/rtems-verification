# Per-function WP benchmark

Drives the verification scripts under `scripts/6.2/`, `scripts/5.1-active/`,
and `scripts/freertos/` once per top-level function (and once per helper),
parses Frama-C/WP's `Proved goals:` / `Qed:` / `Alt-Ergo:` summary, and
renders LaTeX tables matching the paper's existing layout.

Built around three files (with a single-file Python equivalent next to them — see "Python port" below):

| file | role |
|---|---|
| `inside-container.sh`  | Runs inside the FC 32 verification container. Drives every per-function invocation (overriding `WP_FCTS`, passing `-wp-cache none`, optionally `-wp-par $WP_PAR`). Prints a one-line progress bar to stderr per target and emits `RESULT|<label>|<qed>|<alt-ergo>|<total>|<elapsed>|rc=<n>` lines on stdout. |
| `render-table.py`      | Reads two RESULT files (parallel + serial) and renders six LaTeX tables (RTEMS 6.2, RTEMS 5.1, FreeRTOS — parallel and serial). Row composition and folding live in `RTEMS_62_GROUPS` / `RTEMS_51_GROUPS` / `FREERTOS_GROUPS` near the top. |
| `run-and-render.sh`    | Host-side driver. Spins up one container per pass, mirrors logs to `results/logs-{parallel,serial}/`, writes parsed `results/results-{parallel,serial}.txt`, then renders. Streams the progress bar back to the user's terminal while still capturing it to `run-*.err`. |

## Usage

From the repo root:

```bash
# Full benchmark (parallel + serial pass) and LaTeX output:
scripts/bench/run-and-render.sh

# Only one pass:
PASS=parallel scripts/bench/run-and-render.sh
PASS=serial   scripts/bench/run-and-render.sh

# Re-render previously gathered results without re-running:
RENDER_ONLY=1 scripts/bench/run-and-render.sh
```

The serial pass uses `-wp-par 1`; the parallel pass leaves Frama-C's default
`-wp-par 24` alone. All invocations use `-wp-cache none` so wall times are
clean. While each pass runs, progress lines like

```
[bench  17/95] [#####...................]  17%  51_edf_block   ok  18/18 goals   2.45s  (run 1m12s)
```

are streamed to your terminal (and copied into `results/run-{pass}.err`).

## Coverage

`inside-container.sh` runs each verify-*.sh script once *per function*, by
overriding the script's `WP_FCTS` env var. This gives per-function Qed /
Alt-Ergo attribution even though the underlying scripts each verify a small
batch of related functions.

- **RTEMS 6.2** — all of `scripts/6.2/verify-*.sh`, including
  `verify-scheduler-update-priority.sh`, `verify-thread-priority-update.sh`,
  and every helper named in any script's `WP_FCTS`. The uniprocessor unblock
  helpers are driven via a direct frama-c call against
  `verification/6.2/harnesses/scheduleruni-unblock-harness.c` because their
  verify-script hard-codes `-wp-fct`.
- **RTEMS 5.1** — all of `scripts/5.1-active/verify-*.sh`. The
  `_Scheduler_Update_heir` helper (5.1's analog of 6.2's split
  `_Scheduler_uniprocessor_Update_heir*` family) is driven via a direct
  frama-c call against
  `verification/5.1/harnesses/scheduler-update-heir-harness.c`. 5.1-specific
  rows are prefixed `51_` to avoid collisions with the 6.2 names.
- **FreeRTOS** — `vTaskSwitchContext`, `vTaskSuspend`, `vTaskResume`,
  `xTaskDelayUntil` (the fixed body — previously named
  `xTaskDelayUntilReadyRefresh`), `xTaskDelayUntilUnfixed` (the buggy
  reference kept as a negative control — previously named just
  `xTaskDelayUntil`), `xTaskIncrementTick`, and helpers
  `vPortYield`, `prvTaskIsTaskSuspended`, `prvAddCurrentTaskToDelayedList`,
  `xTaskResumeAll`.

`vTaskDelay` is no longer in `verification/freertos/reference/delay.c` and
has been removed from the bench accordingly.

### `-wp-split` and other per-script flags

`-wp-split` is required for `verify-edf-release-cancel.sh` (in both 5.1 and
6.2). It's baked into the script's `WP_FCT_DEFAULTS` and the bench's
`WP_FCTS` override leaves it intact, so any `run_one` call targeting that
script (e.g. `edf_release_job`, `edf_cancel_job`, `h_edf_map_priority`,
`h_edf_unmap_priority`, and the 5.1 equivalents) automatically gets it. No
new wiring is needed unless a future verify script grows a similar
single-script flag.

### Per-goal timeout

The bench passes `-wp-timeout $WP_TIMEOUT` (default **120s**) on every
invocation, overriding the verify scripts' built-in `-wp-timeout 30`
thanks to Frama-C's last-wins semantics. The generous default is for
artifact reviewers on slower hardware — every goal here proves well under
30s on the dev box, but a 4–5× slowdown on a shared cloud VM or older
laptop could push individual goals past 30s, which would falsely fail the
artifact. 120s absorbs roughly a 100× slowdown.

To match the dev workflow exactly:

```bash
WP_TIMEOUT=30 scripts/bench/run-and-render.sh
```

Higher values (`WP_TIMEOUT=300`) are fine too — clean runs are unaffected
since no goal actually approaches the limit.

## Output layout

```
scripts/bench/
├── inside-container.sh
├── render-table.py
├── run-and-render.sh
└── results/
    ├── run-parallel.{out,err}     # raw stdout/stderr of one docker run
    ├── run-serial.{out,err}       # progress bar lives in *.err
    ├── results-parallel.txt       # RESULT lines only — fed into the renderer
    ├── results-serial.txt
    ├── logs-parallel/<label>.log  # verbose Frama-C log per function
    └── logs-serial/<label>.log
```

## What gets counted

- **Function pass only.** Scripts that also run an EDF-property model-lemma
  pass (`verify-edf-{block,schedule,yield,unblock,update-priority,
  release-cancel}.sh` and their 5.1 counterparts) re-prove the same shared
  `edf_ready_set.h` lemma; the parser ignores that section to avoid
  double-counting it across rows.
- **Per-function attribution.** RTEMS scripts honour `WP_FCTS`, so the
  runner sets it to the single function it's attributing goals to. FreeRTOS
  `verify-*-reference.sh` scripts and the uniprocessor/update-heir harnesses
  hard-code `-wp-fct`, so the runner calls `frama-c` directly with the same
  CPP flags but one function at a time.
- **Folding.** The renderer combines:
  - `_Scheduler_EDF_Unblock` + 6.2 `_Scheduler_uniprocessor_Unblock`
    (the EDF entry calls the uniprocessor helper at the end)
  - `_Scheduler_EDF_Unblock` + 5.1 `_Scheduler_Update_heir`
    (5.1's analog of the uniprocessor helper)
  - `_Scheduler_EDF_Release_job` + `_Scheduler_Release_job`
    (the generic wrapper dispatches into the EDF op)
  - 5.1 `_Scheduler_EDF_Cancel_job` + `_Scheduler_Cancel_job` (same pattern)
  - `_Thread_Priority_{add,changed,remove}` into one row
  Edit `RTEMS_62_GROUPS` / `RTEMS_51_GROUPS` / `FREERTOS_GROUPS` in
  `render-table.py` to change which rows are folded or split.

## Caveats

- Requires the `rtems-edf-toolchain-fc32` image (built once via `docker
  compose build verify-6.2-active-fc32`).
- Wall times include both the script's function pass and (where present)
  its model-lemma pass, because the bench times the whole script.
- The bar's denominator (currently 95) is computed by counting `run_*` call
  sites in `inside-container.sh` — add or remove call sites freely; the
  counter and progress bar follow automatically.

## Python port (`bench.py`)

`bench.py` is a stdlib-only Python implementation of the same benchmark.
It produces byte-identical LaTeX and the same `RESULT|...` lines. Both the
bash trio and the Python file remain in the tree — pick whichever you
prefer; the bash trio is the documented reference.

```bash
# Same UX as the bash entry point. PASS / RENDER_ONLY env vars work too.
python3 scripts/bench/bench.py run
python3 scripts/bench/bench.py run --pass parallel
RENDER_ONLY=1 python3 scripts/bench/bench.py run

# Re-render previously gathered results:
python3 scripts/bench/bench.py render \
    --parallel scripts/bench/results/results-parallel.txt \
    --serial   scripts/bench/results/results-serial.txt

# What runs inside the container (don't normally call directly):
python3 scripts/bench/bench.py inside-run
```

Layout: one file, three subcommands. `run` is the host orchestrator
(docker + tee progress to terminal + render); `inside-run` is the
container worker that iterates `TARGETS` and invokes frama-c; `render`
re-runs the LaTeX rendering on previously gathered results. The `TARGETS`
list is a flat declarative `list[Target]` near the top; add or remove
entries there to change coverage.
