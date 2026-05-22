# Per-function WP benchmark

Drives the existing verification scripts under `scripts/6.2/` and
`scripts/freertos/` once per top-level function, parses Frama-C/WP's `Proved
goals:` / `Qed:` / `Alt-Ergo:` summary, and emits LaTeX tables matching the
paper's existing layout.

Built around three files:

| file | role |
|---|---|
| `inside-container.sh`  | Runs inside the FC 32 verification container. Drives every per-function invocation (overriding `WP_FCTS`, passing `-wp-cache none`, optionally `-wp-par $WP_PAR`). Emits `RESULT|<label>|<qed>|<alt-ergo>|<total>|<elapsed>|rc=<n>` lines on stdout. |
| `render-table.py`      | Reads two RESULT files (parallel + serial) and renders four LaTeX tables. Row composition and folding live in `RTEMS_GROUPS`/`FREERTOS_GROUPS` near the top. |
| `run-and-render.sh`    | Host-side driver. Spins up one container per pass, mirrors logs to `results/logs-{parallel,serial}/`, writes parsed `results/results-{parallel,serial}.txt`, then renders. |

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

Both passes take ~2–4 min on this hardware. The serial pass uses
`-wp-par 1`; the parallel pass leaves Frama-C's default `-wp-par 24` alone.
All invocations use `-wp-cache none` so wall times are clean.

## Output layout

```
scripts/bench/
├── inside-container.sh
├── render-table.py
├── run-and-render.sh
└── results/
    ├── run-parallel.{out,err}     # raw stdout/stderr of one docker run
    ├── run-serial.{out,err}
    ├── results-parallel.txt       # RESULT lines only — fed into the renderer
    ├── results-serial.txt
    ├── logs-parallel/<label>.log  # verbose Frama-C log per function
    └── logs-serial/<label>.log
```

## What gets counted

- **Function pass only.** Scripts that also run an EDF-property model-lemma
  pass (`verify-edf-{block,schedule,yield,unblock,update-priority,
  release-cancel}.sh`) re-prove the same shared `edf_ready_set.h` lemma; the
  parser ignores that section to avoid double-counting it across rows.
- **Per-function attribution.** RTEMS scripts honour `WP_FCTS`, so the runner
  sets it to the single function it's attributing goals to. FreeRTOS
  `verify-*-reference.sh` scripts hard-code `-wp-fct`, so the runner calls
  `frama-c` directly with the same CPP flags but one function at a time.
- **Folding.** The renderer combines:
  - `_Scheduler_EDF_Unblock` + `_Scheduler_uniprocessor_Unblock`
    (the EDF entry calls the uniprocessor helper at the end)
  - `_Scheduler_EDF_Release_job` + `_Scheduler_Release_job`
    (the generic wrapper dispatches into the EDF op)
  - `_Thread_Priority_{add,changed,remove}` into one row
  Edit `RTEMS_GROUPS` / `FREERTOS_GROUPS` in `render-table.py` to change which
  rows are folded or split.

## Caveats

- Requires the `rtems-edf-toolchain-fc32` image (built once via `docker
  compose build verify-6.2-active-fc32`).
- Wall times are real time on the host; goal counts come from Frama-C's own
  per-prover summary so they're independent of the timing source.
- All 22 functions must verify clean. If a script grows new unproved goals,
  the parser still records the counts but you should investigate; per-target
  logs in `results/logs-*/<label>.log` have the full Frama-C output.
