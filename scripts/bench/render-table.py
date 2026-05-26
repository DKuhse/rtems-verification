#!/usr/bin/env python3
"""
Reads RESULT lines from one or more pipe-delimited files
(label|qed|alt|total|elapsed|rc=...) and renders LaTeX tables matching
the paper's existing layout.

Folding is data: GROUPS lists (display_label, [run_labels...]) tuples.
Multi-label entries sum proof goals and wall time across listed labels.
Labels measured by the runner but not listed in any group are silently
dropped (e.g. `xTaskDelayUntilUnfixed` is kept as a negative reference
in the results file but not surfaced in the rendered table).
"""
import pathlib, argparse

DEFAULT_DIR = pathlib.Path("scripts/bench/results")

# ─── Helper-row aggregates ──────────────────────────────────────────────────────
# `Scheduler helpers` sums every EDF / uniprocessor-update-heir helper that is
# measured individually. `Priority helpers` sums every priority-tree helper
# plus the standalone `_Thread_Priority_update` composition step (which the
# old table folded into the priority side rather than giving it its own row).

EDF_HELPERS_62 = [
    "h_edf_get_context", "h_edf_node_downcast",
    "h_edf_map_priority", "h_edf_unmap_priority",
    "h_scheduler_get_context", "h_rbtree_init_empty",
    "h_node_do_initialize", "h_rbtree_init_node",
    "h_thread_is_ready",
    "h_uni_update_heir", "h_uni_update_heir_if_necessary",
    "h_uni_update_heir_if_preemptible",
]

PRIORITY_HELPERS_62 = [
    "h_priority_actions_add", "h_priority_non_empty_insert",
    "h_priority_extract_non_empty", "h_priority_changed",
    "h_thread_set_sched_node_prio", "h_thread_priority_action_change",
    "h_thread_queue_do_nothing_pa", "h_thread_priority_do_perform",
    "h_thread_priority_apply", "h_scheduler_node_set_priority",
    # The composition step folds in here (no dedicated row in the old format).
    "thread_priority_update",
]

# Row order and short labels match the paper's RTEMS 6 table.
RTEMS_62_GROUPS = [
    (r"Initialize",              ["edf_initialize"]),
    (r"Node\_initialize",        ["edf_node_initialize"]),
    (r"Block",                   ["edf_block"]),
    (r"Schedule",                ["edf_schedule"]),
    (r"Yield",                   ["edf_yield"]),
    (r"Unblock",                 ["edf_unblock", "scheduleruni_unblock"]),
    (r"Update\_priority",        ["edf_update_priority", "scheduler_update_priority"]),
    (r"Release\_job",            ["edf_release_job", "scheduler_release_job"]),
    (r"Cancel\_job",             ["edf_cancel_job"]),
    (r"Thread\_Priority\_\{*\}", ["thread_priority_add",
                                  "thread_priority_changed",
                                  "thread_priority_remove"]),
    (r"RM\_Release\_job",        ["ratemon_release_job"]),
    (r"RM\_Cancel",              ["ratemon_cancel"]),
    (r"Scheduler helpers",       EDF_HELPERS_62),
    (r"Priority helpers",        PRIORITY_HELPERS_62),
]

# 5.1 mirror. The 5.1 source doesn't split unblock into uniprocessor+EDF, so
# the EDF entry rolls the `_Scheduler_Update_heir` helper into Unblock here.
# `_Scheduler_Cancel_job` is the 5.1 generic wrapper that 6.2 doesn't have.

EDF_HELPERS_51 = [
    "51_h_edf_get_context", "51_h_edf_node_downcast",
    "51_h_edf_map_priority", "51_h_edf_unmap_priority",
    "51_h_scheduler_get_context", "51_h_rbtree_init_empty",
    "51_h_node_do_initialize", "51_h_rbtree_init_node",
    "51_h_edf_schedule_body", "51_h_edf_extract_body",
    "51_h_thread_is_ready", "51_h_scheduler_node_get_prio",
    "51_h_thread_get_priority", "51_h_thread_sched_home_node",
    "51_h_priority_get_priority",
    "51_h_thread_get_cpu",
]

PRIORITY_HELPERS_51 = [
    "51_h_priority_actions_add", "51_h_priority_non_empty_insert",
    "51_h_priority_extract_non_empty", "51_h_priority_changed",
    "51_h_thread_set_sched_node_prio", "51_h_thread_priority_action_change",
    "51_h_thread_queue_do_nothing_pa", "51_h_thread_priority_do_perform",
    "51_h_thread_priority_apply", "51_h_scheduler_node_set_priority",
    "51_thread_priority_update",
]

RTEMS_51_GROUPS = [
    (r"Initialize",              ["51_edf_initialize"]),
    (r"Node\_initialize",        ["51_edf_node_initialize"]),
    (r"Block",                   ["51_edf_block"]),
    (r"Schedule",                ["51_edf_schedule"]),
    (r"Yield",                   ["51_edf_yield"]),
    (r"Unblock",                 ["51_edf_unblock", "51_scheduler_update_heir"]),
    (r"Update\_priority",        ["51_edf_update_priority", "51_scheduler_update_priority"]),
    (r"Release\_job",            ["51_edf_release_job", "51_scheduler_release_job"]),
    (r"Cancel\_job",             ["51_edf_cancel_job", "51_scheduler_cancel_job"]),
    (r"Thread\_Priority\_\{*\}", ["51_thread_priority_add",
                                  "51_thread_priority_changed",
                                  "51_thread_priority_remove"]),
    (r"RM\_Release\_job",        ["51_ratemon_release_job"]),
    (r"RM\_Cancel",              ["51_ratemon_cancel"]),
    (r"Scheduler helpers",       EDF_HELPERS_51),
    (r"Priority helpers",        PRIORITY_HELPERS_51),
]

# FreeRTOS — `xTaskDelayUntilUnfixed` is measured (it's in TARGETS for
# auditability of the negative reference) but intentionally not surfaced here.
FREERTOS_GROUPS = [
    (r"vTaskSwitchContext",  ["vTaskSwitchContext"]),
    (r"vTaskSuspend",        ["vTaskSuspend"]),
    (r"vTaskResume",         ["vTaskResume"]),
    (r"xTaskDelayUntil",     ["xTaskDelayUntil"]),
    (r"xTaskIncrementTick",  ["xTaskIncrementTick"]),
    (r"Task helpers", [
        "h_vPortYield", "h_prvTaskIsTaskSuspended",
        "h_prvAddCurrentTaskToDelayedList", "h_xTaskResumeAll",
    ]),
]

# ─── Loader ────────────────────────────────────────────────────────────────────

def load(path: pathlib.Path) -> dict:
    data = {}
    if not path.exists():
        return data
    for line in path.read_text().splitlines():
        if not line.startswith("RESULT|"):
            continue
        _, label, qed, alt, total, elapsed, rc = line.split("|", 6)
        data[label] = {
            "qed": int(qed),
            "alt": int(alt),
            "total": int(total),
            "elapsed": float(elapsed),
            "rc": rc.removeprefix("rc="),
        }
    return data

def fold(labels, data, missing_sink=None):
    qed = alt = total = 0
    elapsed = 0.0
    for label in labels:
        if label not in data:
            if missing_sink is not None:
                missing_sink.add(label)
            continue
        d = data[label]
        qed     += d["qed"]
        alt     += d["alt"]
        total   += d["total"]
        elapsed += d["elapsed"]
    return qed, alt, total, elapsed


# ─── Rendering ─────────────────────────────────────────────────────────────────
# Matches the paper's column widths (p{0.38} label / p{0.05} qed /
# p{0.13} alt / p{0.20} time) and wraps each tabular in a full table env
# with caption + label so the output can be \input{...} directly.

TABULAR_PREAMBLE = (
    r"\begin{tabular}{>{\raggedright}p{0.38\linewidth}"
    r" >{\centering}p{0.05\linewidth}"
    r">{\centering}p{0.13\linewidth}"
    r">{\centering\arraybackslash}p{0.20\linewidth}}"
)

def render(caption: str, label: str, groups, data) -> str:
    missing = set()
    out: list[str] = []
    out.append(r"\begin{table}[tp]")
    out.append(r"    \small")
    out.append(r"    \centering")
    out.append(f"    \\caption{{{caption}}}")
    out.append(f"    \\label{{{label}}}")
    out.append(TABULAR_PREAMBLE)
    out.append(r"    & \multicolumn{2}{c}{\textbf{Proof Goals}} & \multirow{2}{*}{\textbf{Time}}\\ \cmidrule{2-3}")
    out.append(r"    & Qed & Alt-Ergo & \\ \hline \hline")
    tq = ta = 0
    tt = 0.0
    for display, labels in groups:
        qed, alt, _, elapsed = fold(labels, data, missing_sink=missing)
        tq += qed; ta += alt; tt += elapsed
        out.append(f"    \\texttt{{{display}}} & {qed} & {alt} & {{$\\approx {elapsed:.1f}\\,s$}} \\\\")
    out.append(r"    \midrule")
    out.append(f"    \\textbf{{Total}} & \\textbf{{{tq}}} & \\textbf{{{ta}}} & {{$\\approx {tt:.1f}\\,s$}} \\\\")
    out.append(r"\end{tabular}")
    out.append(r"\end{table}")
    if missing:
        out.insert(0, f"% WARNING: missing RESULT labels in source CSV: {sorted(missing)}")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--parallel", default=str(DEFAULT_DIR/"results-parallel.txt"),
                    help="path to parallel-run RESULT file")
    ap.add_argument("--serial",   default=str(DEFAULT_DIR/"results-serial.txt"),
                    help="path to serial-run RESULT file (-wp-par 1)")
    args = ap.parse_args()

    par = load(pathlib.Path(args.parallel))
    ser = load(pathlib.Path(args.serial))

    sections = [
        ("Derived proof goals and required time of RTEMS 6 verification.",
            "tab:rtems6-stats", RTEMS_62_GROUPS, par),
        ("Derived proof goals and required time of RTEMS 6 verification (sequential).",
            "tab:rtems6-stats-seq", RTEMS_62_GROUPS, ser),
        ("Derived proof goals and required time of RTEMS 5 verification.",
            "tab:rtems5-stats", RTEMS_51_GROUPS, par),
        ("Derived proof goals and required time of RTEMS 5 verification (sequential).",
            "tab:rtems5-stats-seq", RTEMS_51_GROUPS, ser),
        ("Derived proof goals and required time of FreeRTOS verification.",
            "tab:freertos-stats", FREERTOS_GROUPS, par),
        ("Derived proof goals and required time of FreeRTOS verification (sequential).",
            "tab:freertos-stats-seq", FREERTOS_GROUPS, ser),
    ]
    for i, (caption, label, groups, data) in enumerate(sections):
        if i:
            print()
        print(render(caption, label, groups, data))

if __name__ == "__main__":
    main()
