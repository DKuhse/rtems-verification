#!/usr/bin/env python3
"""
Reads RESULT lines from one or more pipe-delimited files
(label|qed|alt|total|elapsed|rc=...) and renders LaTeX tables.

Folding is data: GROUPS lists (display_label, [run_labels...]) tuples.
Multi-label entries sum proof goals and wall time across listed labels.
"""
import pathlib, argparse

DEFAULT_DIR = pathlib.Path("scripts/bench/results")

RTEMS_GROUPS = [
    (r"\_Scheduler\_EDF\_Initialize",          ["edf_initialize"]),
    (r"\_Scheduler\_EDF\_Node\_initialize",    ["edf_node_initialize"]),
    (r"\_Scheduler\_EDF\_Block",               ["edf_block"]),
    (r"\_Scheduler\_EDF\_Schedule",            ["edf_schedule"]),
    (r"\_Scheduler\_EDF\_Yield",               ["edf_yield"]),
    (r"\_Scheduler\_EDF\_Unblock (incl.\ uniprocessor unblock)",
                                               ["edf_unblock", "scheduleruni_unblock"]),
    (r"\_Scheduler\_EDF\_Update\_priority",    ["edf_update_priority"]),
    (r"\_Scheduler\_EDF\_Release\_job (incl.\ \_Scheduler\_Release\_job)",
                                               ["edf_release_job", "scheduler_release_job"]),
    (r"\_Scheduler\_EDF\_Cancel\_job",         ["edf_cancel_job"]),
    (r"\_Thread\_Priority\_\{add, changed, remove\}",
                                               ["thread_priority_add",
                                                "thread_priority_changed",
                                                "thread_priority_remove"]),
    (r"\_Rate\_monotonic\_Release\_job",       ["ratemon_release_job"]),
    (r"\_Rate\_monotonic\_Cancel",             ["ratemon_cancel"]),
    # Helper rows -- each helper verified once, summed here.
    (r"EDF scheduler helpers", [
        "h_edf_get_context", "h_edf_node_downcast",
        "h_edf_map_priority", "h_edf_unmap_priority",
        "h_scheduler_get_context", "h_rbtree_init_empty",
        "h_node_do_initialize", "h_rbtree_init_node",
        "h_thread_is_ready",
        "h_uni_update_heir", "h_uni_update_heir_if_necessary",
        "h_uni_update_heir_if_preemptible",
    ]),
    (r"Priority helpers", [
        "h_priority_actions_add", "h_priority_non_empty_insert",
        "h_priority_extract_non_empty", "h_priority_changed",
        "h_thread_set_sched_node_prio", "h_thread_priority_action_change",
    ]),
]

FREERTOS_GROUPS = [
    (r"vTaskSwitchContext",  ["vTaskSwitchContext"]),
    (r"vTaskSuspend",        ["vTaskSuspend"]),
    (r"vTaskResume",         ["vTaskResume"]),
    # The verified/fixed body of xTaskDelayUntil is xTaskDelayUntilReadyRefresh.
    (r"xTaskDelayUntil",     ["h_xTaskDelayUntilReadyRefresh"]),
    (r"xTaskIncrementTick",  ["xTaskIncrementTick"]),
    (r"Task helpers", [
        "h_vPortYield", "h_prvTaskIsTaskSuspended",
        "h_prvAddCurrentTaskToDelayedList", "h_xTaskResumeAll",
    ]),
]

def load(path: pathlib.Path) -> dict:
    data = {}
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

def render(title, groups, data):
    missing = set()
    out = [f"% {title}"]
    out.append(r"\begin{tabular}{>{\raggedright}p{0.55\linewidth} >{\centering}p{0.05\linewidth}>{\centering}p{0.13\linewidth}>{\centering\arraybackslash}p{0.13\linewidth}}")
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
    if missing:
        out.insert(1, f"% WARNING: missing RESULT labels in source CSV: {sorted(missing)}")
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

    print(render("RTEMS 6.2 - per-function WP proof goals (parallel, default -wp-par 24)",
                 RTEMS_GROUPS, par))
    print()
    print(render("RTEMS 6.2 - per-function WP proof goals (serial, -wp-par 1)",
                 RTEMS_GROUPS, ser))
    print()
    print(render("FreeRTOS - per-function WP proof goals (parallel, default -wp-par 24)",
                 FREERTOS_GROUPS, par))
    print()
    print(render("FreeRTOS - per-function WP proof goals (serial, -wp-par 1)",
                 FREERTOS_GROUPS, ser))

if __name__ == "__main__":
    main()
