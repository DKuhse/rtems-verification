#!/usr/bin/env python3
"""Per-function WP benchmark — Python port.

This is the data-driven Python equivalent of the bash trio:
    inside-container.sh + run-and-render.sh + render-table.py

The bash version remains the documented reference and is functionally
identical. Pick whichever you prefer; both write the same RESULT lines and
the same LaTeX tables.

Subcommands:
    run         host-side: spin up docker per pass, parse, render
    inside-run  container-side: iterate targets, drive frama-c
    render      re-render previously gathered results files

Compat:
    PASS=parallel|serial|both         (same as the bash flag)
    RENDER_ONLY=1                     (same as the bash flag)
    IMAGE=<docker image>              (default rtems-edf-toolchain-fc32)
    WP_PAR=<n>                        (set by `run`; honoured by `inside-run`)
    WP_TIMEOUT=<s>                    per-goal prover timeout (default 120s,
                                      generous on purpose for slow artifact
                                      reviewer hardware; verify scripts use
                                      30s for the dev workflow)
    LOG_DIR=<dir>                     (override /tmp/wp-logs inside container)
"""
from __future__ import annotations

import argparse
import dataclasses
import os
import pathlib
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from typing import List, Optional, Tuple


# ─── Target list ────────────────────────────────────────────────────────────────
# Each target picks ONE function to attribute goals to. Drivers:
#   "script"        – run WP_FCTS=<fct> <script> -wp-cache none. Scripts honour
#                     WP_FCTS; per-script flags like -wp-split ride along in
#                     the script's own WP_FCT_DEFAULTS untouched.
#   "uni_helper"    – direct frama-c against the 6.2 scheduleruni-unblock
#                     harness (verify-script hard-codes -wp-fct).
#   "heir51_helper" – same idea for the 5.1 scheduler-update-heir harness.
#   "freertos"      – direct frama-c against a freertos reference slice.

@dataclass(frozen=True)
class Target:
    driver: str
    label: str
    script: Optional[str] = None   # for driver="script"
    fct: Optional[str] = None      # always — single WP function name
    source: Optional[str] = None   # for driver="freertos" (path under verification/freertos/)


def _62(label, script_base, fct):
    return Target("script", label,
                  script=f"/opt/scripts/6.2/{script_base}", fct=fct)

def _51(label, script_base, fct):
    return Target("script", label,
                  script=f"/opt/scripts/5.1-active/{script_base}", fct=fct)

def _uni(label, fct):
    return Target("uni_helper", label, fct=fct)

def _heir51(label, fct):
    return Target("heir51_helper", label, fct=fct)

def _fr(label, source_rel, fct):
    return Target("freertos", label, source=source_rel, fct=fct)


TARGETS: List[Target] = [
    # ─── RTEMS 6.2 top-level entry points ──────────────────────────────────────
    _62("edf_initialize",            "verify-edf-initialize.sh",          "_Scheduler_EDF_Initialize"),
    _62("edf_node_initialize",       "verify-edf-node-initialize.sh",     "_Scheduler_EDF_Node_initialize"),
    _62("edf_block",                 "verify-edf-block.sh",               "_Scheduler_EDF_Block"),
    _62("edf_schedule",              "verify-edf-schedule.sh",            "_Scheduler_EDF_Schedule"),
    _62("edf_yield",                 "verify-edf-yield.sh",               "_Scheduler_EDF_Yield"),
    _62("edf_unblock",               "verify-edf-unblock.sh",             "_Scheduler_EDF_Unblock"),
    _62("edf_update_priority",       "verify-edf-update-priority.sh",     "_Scheduler_EDF_Update_priority"),
    _62("edf_release_job",           "verify-edf-release-cancel.sh",      "_Scheduler_EDF_Release_job"),
    _62("edf_cancel_job",            "verify-edf-release-cancel.sh",      "_Scheduler_EDF_Cancel_job"),
    _uni("scheduleruni_unblock",     "_Scheduler_uniprocessor_Unblock"),
    _62("scheduler_release_job",     "verify-scheduler-release-job.sh",   "_Scheduler_Release_job"),
    _62("scheduler_update_priority", "verify-scheduler-update-priority.sh","_Scheduler_Update_priority"),
    _62("thread_priority_add",       "verify-thread-change-priority.sh",  "_Thread_Priority_add"),
    _62("thread_priority_changed",   "verify-thread-change-priority.sh",  "_Thread_Priority_changed"),
    _62("thread_priority_remove",    "verify-thread-change-priority.sh",  "_Thread_Priority_remove"),
    _62("thread_priority_update",    "verify-thread-priority-update.sh",  "_Thread_Priority_update"),
    _62("ratemon_release_job",       "verify-ratemon-release-job.sh",     "_Rate_monotonic_Release_job"),
    _62("ratemon_cancel",            "verify-ratemon-cancel.sh",          "_Rate_monotonic_Cancel"),

    # ─── RTEMS 6.2 EDF scheduler helpers ───────────────────────────────────────
    _62("h_edf_get_context",         "verify-edf-block.sh",              "_Scheduler_EDF_Get_context"),
    _62("h_edf_node_downcast",       "verify-edf-block.sh",              "_Scheduler_EDF_Node_downcast"),
    _62("h_edf_map_priority",        "verify-edf-release-cancel.sh",     "_Scheduler_EDF_Map_priority"),
    _62("h_edf_unmap_priority",      "verify-edf-release-cancel.sh",     "_Scheduler_EDF_Unmap_priority"),
    _62("h_scheduler_get_context",   "verify-edf-initialize.sh",         "_Scheduler_Get_context"),
    _62("h_rbtree_init_empty",       "verify-edf-initialize.sh",         "_RBTree_Initialize_empty"),
    _62("h_node_do_initialize",      "verify-edf-node-initialize.sh",    "_Scheduler_Node_do_initialize"),
    _62("h_rbtree_init_node",        "verify-edf-node-initialize.sh",    "_RBTree_Initialize_node"),
    _62("h_thread_is_ready",         "verify-edf-schedule.sh",           "_Thread_Is_ready"),
    _uni("h_uni_update_heir",                "_Scheduler_uniprocessor_Update_heir"),
    _uni("h_uni_update_heir_if_necessary",   "_Scheduler_uniprocessor_Update_heir_if_necessary"),
    _uni("h_uni_update_heir_if_preemptible", "_Scheduler_uniprocessor_Update_heir_if_preemptible"),

    # ─── RTEMS 6.2 priority helpers ────────────────────────────────────────────
    _62("h_priority_actions_add",          "verify-thread-change-priority.sh", "_Priority_Actions_add"),
    _62("h_priority_non_empty_insert",     "verify-thread-change-priority.sh", "_Priority_Non_empty_insert"),
    _62("h_priority_extract_non_empty",    "verify-thread-change-priority.sh", "_Priority_Extract_non_empty"),
    _62("h_priority_changed",              "verify-thread-change-priority.sh", "_Priority_Changed"),
    _62("h_thread_set_sched_node_prio",    "verify-thread-change-priority.sh", "_Thread_Set_scheduler_node_priority"),
    _62("h_thread_priority_action_change", "verify-thread-change-priority.sh", "_Thread_Priority_action_change"),
    _62("h_thread_queue_do_nothing_pa",    "verify-thread-change-priority.sh", "_Thread_queue_Do_nothing_priority_actions"),
    _62("h_thread_priority_do_perform",    "verify-thread-change-priority.sh", "_Thread_Priority_do_perform_actions"),
    _62("h_thread_priority_apply",         "verify-thread-change-priority.sh", "_Thread_Priority_apply"),
    _62("h_scheduler_node_set_priority",   "verify-thread-change-priority.sh", "_Scheduler_Node_set_priority"),

    # ─── RTEMS 5.1 top-level entry points ──────────────────────────────────────
    _51("51_edf_initialize",            "verify-edf-initialize.sh",          "_Scheduler_EDF_Initialize"),
    _51("51_edf_node_initialize",       "verify-edf-node-initialize.sh",     "_Scheduler_EDF_Node_initialize"),
    _51("51_edf_block",                 "verify-edf-block.sh",               "_Scheduler_EDF_Block"),
    _51("51_edf_schedule",              "verify-edf-schedule.sh",            "_Scheduler_EDF_Schedule"),
    _51("51_edf_yield",                 "verify-edf-yield.sh",               "_Scheduler_EDF_Yield"),
    _51("51_edf_unblock",               "verify-edf-unblock.sh",             "_Scheduler_EDF_Unblock"),
    _51("51_edf_update_priority",       "verify-edf-update-priority.sh",     "_Scheduler_EDF_Update_priority"),
    _51("51_edf_release_job",           "verify-edf-release-cancel.sh",      "_Scheduler_EDF_Release_job"),
    _51("51_edf_cancel_job",            "verify-edf-release-cancel.sh",      "_Scheduler_EDF_Cancel_job"),
    _heir51("51_scheduler_update_heir", "_Scheduler_Update_heir"),
    _51("51_scheduler_release_job",     "verify-scheduler-release-job.sh",     "_Scheduler_Release_job"),
    _51("51_scheduler_cancel_job",      "verify-scheduler-cancel-job.sh",      "_Scheduler_Cancel_job"),
    _51("51_scheduler_update_priority", "verify-scheduler-update-priority.sh", "_Scheduler_Update_priority"),
    _51("51_thread_priority_add",       "verify-thread-change-priority.sh",    "_Thread_Priority_add"),
    _51("51_thread_priority_changed",   "verify-thread-change-priority.sh",    "_Thread_Priority_changed"),
    _51("51_thread_priority_remove",    "verify-thread-change-priority.sh",    "_Thread_Priority_remove"),
    _51("51_thread_priority_update",    "verify-thread-priority-update.sh",    "_Thread_Priority_update"),
    _51("51_ratemon_release_job",       "verify-ratemon-release-job.sh",       "_Rate_monotonic_Release_job"),
    _51("51_ratemon_cancel",            "verify-ratemon-cancel.sh",            "_Rate_monotonic_Cancel"),

    # ─── RTEMS 5.1 EDF scheduler helpers ───────────────────────────────────────
    _51("51_h_edf_get_context",          "verify-edf-block.sh",            "_Scheduler_EDF_Get_context"),
    _51("51_h_edf_node_downcast",        "verify-edf-block.sh",            "_Scheduler_EDF_Node_downcast"),
    _51("51_h_edf_map_priority",         "verify-edf-map-unmap.sh",        "_Scheduler_EDF_Map_priority"),
    _51("51_h_edf_unmap_priority",       "verify-edf-map-unmap.sh",        "_Scheduler_EDF_Unmap_priority"),
    _51("51_h_scheduler_get_context",    "verify-edf-initialize.sh",       "_Scheduler_Get_context"),
    _51("51_h_rbtree_init_empty",        "verify-edf-initialize.sh",       "_RBTree_Initialize_empty"),
    _51("51_h_node_do_initialize",       "verify-edf-node-initialize.sh",  "_Scheduler_Node_do_initialize"),
    _51("51_h_rbtree_init_node",         "verify-edf-node-initialize.sh",  "_RBTree_Initialize_node"),
    _51("51_h_edf_schedule_body",        "verify-edf-schedule.sh",         "_Scheduler_EDF_Schedule_body"),
    _51("51_h_edf_extract_body",         "verify-edf-schedule.sh",         "_Scheduler_EDF_Extract_body"),
    _51("51_h_thread_is_ready",          "verify-edf-update-priority.sh",  "_Thread_Is_ready"),
    _51("51_h_scheduler_node_get_prio",  "verify-edf-update-priority.sh",  "_Scheduler_Node_get_priority"),
    _51("51_h_thread_get_priority",      "verify-edf-unblock.sh",          "_Thread_Get_priority"),
    _51("51_h_thread_sched_home_node",   "verify-edf-unblock.sh",          "_Thread_Scheduler_get_home_node"),
    _51("51_h_priority_get_priority",    "verify-edf-unblock.sh",          "_Priority_Get_priority"),
    _heir51("51_h_thread_get_cpu",       "_Thread_Get_CPU"),

    # ─── RTEMS 5.1 priority helpers ────────────────────────────────────────────
    _51("51_h_priority_actions_add",          "verify-thread-change-priority.sh", "_Priority_Actions_add"),
    _51("51_h_priority_non_empty_insert",     "verify-thread-change-priority.sh", "_Priority_Non_empty_insert"),
    _51("51_h_priority_extract_non_empty",    "verify-thread-change-priority.sh", "_Priority_Extract_non_empty"),
    _51("51_h_priority_changed",              "verify-thread-change-priority.sh", "_Priority_Changed"),
    _51("51_h_thread_set_sched_node_prio",    "verify-thread-change-priority.sh", "_Thread_Set_scheduler_node_priority"),
    _51("51_h_thread_priority_action_change", "verify-thread-change-priority.sh", "_Thread_Priority_action_change"),
    _51("51_h_thread_queue_do_nothing_pa",    "verify-thread-change-priority.sh", "_Thread_queue_Do_nothing_priority_actions"),
    _51("51_h_thread_priority_do_perform",    "verify-thread-change-priority.sh", "_Thread_Priority_do_perform_actions"),
    _51("51_h_thread_priority_apply",         "verify-thread-change-priority.sh", "_Thread_Priority_apply"),
    _51("51_h_scheduler_node_set_priority",   "verify-thread-change-priority.sh", "_Scheduler_Node_set_priority"),

    # ─── FreeRTOS reference top-level entry points ────────────────────────────
    _fr("vTaskSwitchContext",     "reference/taskswitchcontext.c", "vTaskSwitchContext"),
    _fr("vTaskSuspend",           "reference/suspend.c",           "vTaskSuspend"),
    _fr("vTaskResume",            "reference/resume.c",            "vTaskResume"),
    _fr("xTaskDelayUntil",        "reference/delay.c",             "xTaskDelayUntil"),
    _fr("xTaskDelayUntilUnfixed", "reference/delay.c",             "xTaskDelayUntilUnfixed"),
    _fr("xTaskIncrementTick",     "reference/incrementtick.c",     "xTaskIncrementTick"),

    # ─── FreeRTOS task helpers ────────────────────────────────────────────────
    _fr("h_vPortYield",                     "reference/suspend.c", "vPortYield"),
    _fr("h_prvTaskIsTaskSuspended",         "reference/resume.c",  "prvTaskIsTaskSuspended"),
    _fr("h_prvAddCurrentTaskToDelayedList", "reference/delay.c",   "prvAddCurrentTaskToDelayedList"),
    _fr("h_xTaskResumeAll",                 "reference/delay.c",   "xTaskResumeAll"),
]


# ─── Render groups ──────────────────────────────────────────────────────────────
# Identical to scripts/bench/render-table.py — keep in sync. The bash version
# is the canonical source; this is a mirror.
#
# Row labels and folding match the paper's old table format. New entries
# (`_Scheduler_Update_priority`, `_Thread_Priority_update`, the four new
# priority helpers I backfilled in 6.2, and every 5.1 row) are folded into
# the corresponding `Update_priority` / `Release_job` / `Cancel_job` /
# `Thread_Priority_{*}` / `Priority helpers` rows rather than spawning
# new dedicated rows. `xTaskDelayUntilUnfixed` is measured but not surfaced
# (kept as a negative reference in the results file only).

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
    # Composition step — no dedicated row in the old format.
    "thread_priority_update",
]

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

# `xTaskDelayUntilUnfixed` is measured (it stays in TARGETS for auditability
# of the negative reference) but intentionally not surfaced in the table.
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


# ─── Inside-container drivers ───────────────────────────────────────────────────

def _env(name: str, default: str) -> str:
    return os.environ.get(name, default)

# These mirror the bash defaults exactly.
RTEMS_SRC_62    = _env("RTEMS_SRC_62", "/workspace/rtems/src/rtems-6.2-pristine")
RTEMS_SRC_51    = _env("RTEMS_SRC_51", "/workspace/rtems/src/rtems-5.1-pristine")
RTEMS_PREFIX    = _env("RTEMS_PREFIX", "/opt/rtems5")
OVERLAY_62      = _env("OVERLAY_62",   "/workspace/verification/6.2")
OVERLAY_51      = _env("OVERLAY_51",   "/workspace/verification/5.1")
RTEMS_BUILD_BSP = _env("RTEMS_BUILD_BSP",
                       "/workspace/rtems/build/amd64/x86_64-rtems5/c/amd64/include")
FREERTOS_SRC      = "/workspace/source/freertos-edf-msp430"
FREERTOS_OVERLAY  = "/workspace/verification/freertos"

# WP_TIMEOUT (default 120s) — per-goal prover timeout.
# The verify scripts hard-code 30s for the dev workflow; this artifact
# runner uses a generous default so the bench doesn't misfire on slower
# reviewer hardware. We always append a fresh -wp-timeout to every
# invocation, and Frama-C uses the last value it sees on the command line.
WP_TIMEOUT = _env("WP_TIMEOUT", "120")


def _wp_extra() -> List[str]:
    extra = ["-wp-timeout", WP_TIMEOUT]
    par = os.environ.get("WP_PAR")
    if par:
        extra += ["-wp-par", par]
    return extra


def _rtems_cpp_cmd(overlay: str, rtems_src: str) -> str:
    parts = [
        f"{RTEMS_PREFIX}/bin/x86_64-rtems5-gcc -C -E",
        "-D__FRAMAC__ -D__rtems__",
        f"-I{overlay}/overlay/cpukit/include",
        f"-I{overlay}/models",
        f"-I{rtems_src}/cpukit/include",
        f"-I{rtems_src}/cpukit/score/cpu/x86_64/include",
        f"-I{RTEMS_BUILD_BSP}",
        f"-I{RTEMS_PREFIX}/x86_64-rtems5/include",
        f"-I{RTEMS_PREFIX}/lib/gcc/x86_64-rtems5/9.3.0/include",
        f"-I{rtems_src}/bsps/include",
        f"-I{rtems_src}/bsps/x86_64/include",
        f"-I{rtems_src}/bsps/x86_64/amd64/include",
        "-nostdinc",
    ]
    return " ".join(parts)


def _freertos_cpp_cmd() -> str:
    return " ".join([
        "gcc -C -E",
        "-D__LARGE_DATA_MODEL__ -D__FRAMAC__ -DEDF_SCHEDULER=1",
        f"-I{FREERTOS_OVERLAY}/model",
        f"-I{FREERTOS_OVERLAY}/overlay/include",
        f"-I{FREERTOS_OVERLAY}/stubs",
        f"-I{FREERTOS_SRC}/include",
        "-nostdinc",
        "-isystem /usr/include",
        "-isystem /usr/include/x86_64-linux-gnu",
        "-isystem /usr/lib/gcc/x86_64-linux-gnu/11/include",
    ])


def _run(cmd: List[str], log_path: pathlib.Path, env=None) -> int:
    with log_path.open("w") as logf:
        return subprocess.run(cmd, env=env, stdout=logf,
                              stderr=subprocess.STDOUT).returncode


def _drive_script(t: Target, log_path: pathlib.Path) -> int:
    env = os.environ.copy()
    env["WP_FCTS"] = t.fct  # type: ignore[assignment]
    # _wp_extra() appends -wp-timeout (and -wp-par if set); the verify
    # script's built-in -wp-timeout 30 is overridden by Frama-C's last-wins.
    cmd = [t.script, "-wp-cache", "none", *_wp_extra()]
    return _run(cmd, log_path, env=env)


def _drive_direct(source: str, fct: str, log_path: pathlib.Path,
                  cpp_cmd: str, machdep: str) -> int:
    cmd = [
        "frama-c",
        "-cpp-command", cpp_cmd,
        "-machdep", machdep,
        "-cpp-frama-c-compliant", "-std", "c11",
        source,
        "-volatile", "-then-on", "Volatile",
        "-wp", "-wp-fct", fct,
        "-wp-model", "Typed+Cast",
        "-wp-cache", "none",
        *_wp_extra(),
    ]
    return _run(cmd, log_path)


def run_target(t: Target, log_dir: pathlib.Path) -> Tuple[int, float]:
    log_path = log_dir / f"{t.label}.log"
    t0 = time.time()
    if t.driver == "script":
        rc = _drive_script(t, log_path)
    elif t.driver == "uni_helper":
        src = f"{OVERLAY_62}/harnesses/scheduleruni-unblock-harness.c"
        rc = _drive_direct(src, t.fct, log_path,                       # type: ignore[arg-type]
                           _rtems_cpp_cmd(OVERLAY_62, RTEMS_SRC_62),
                           "gcc_x86_64")
    elif t.driver == "heir51_helper":
        src = f"{OVERLAY_51}/harnesses/scheduler-update-heir-harness.c"
        rc = _drive_direct(src, t.fct, log_path,                       # type: ignore[arg-type]
                           _rtems_cpp_cmd(OVERLAY_51, RTEMS_SRC_51),
                           "gcc_x86_64")
    elif t.driver == "freertos":
        src = f"{FREERTOS_OVERLAY}/{t.source}"
        rc = _drive_direct(src, t.fct, log_path,                       # type: ignore[arg-type]
                           _freertos_cpp_cmd(), "gcc_x86_16")
    else:
        raise ValueError(f"unknown driver: {t.driver}")
    return rc, time.time() - t0


# ─── Log parser ─────────────────────────────────────────────────────────────────

_RE_PROVED = re.compile(r"^\[wp\] Proved goals:\s*(\d+)\s*/\s*(\d+)", re.M)


def _first_int_token(line: str) -> int:
    """Return the first whitespace-separated pure-integer token, or 0.

    Mirrors the bash parser's awk:
        for(i=1;i<=NF;i++) if($i ~ /^[0-9]+$/){print $i; exit}
    This skips version-style tokens like "2.6.2:" while picking up the
    real count that follows.
    """
    for tok in line.split():
        if tok.isdigit():
            return int(tok)
    return 0


def parse_log(log_path: pathlib.Path) -> Tuple[int, int, int]:
    """Return (qed, alt_ergo, total) parsed from the function-pass section.

    When the script emits `=== ... function ===` / `=== ... model lemma ===`
    markers (the EDF scripts do, for the lemma pass that's shared across rows
    and shouldn't be double-counted), only the function section is parsed.
    Otherwise the whole log is parsed.
    """
    try:
        text = log_path.read_text(errors="replace")
    except FileNotFoundError:
        return (0, 0, 0)
    fn_lines: List[str] = []
    in_fn = False
    saw_markers = False
    for line in text.splitlines():
        if "=== " in line and " function ===" in line:
            in_fn = True
            saw_markers = True
            continue
        if "=== " in line and " model lemma ===" in line:
            in_fn = False
            saw_markers = True
            continue
        if in_fn:
            fn_lines.append(line)
    fnpass_lines = fn_lines if saw_markers else text.splitlines()

    total = qed = alt = 0
    fnpass = "\n".join(fnpass_lines)
    if (m := _RE_PROVED.search(fnpass)):
        total = int(m.group(2))
    for line in fnpass_lines:
        stripped = line.lstrip()
        if qed == 0 and stripped.startswith("Qed:"):
            qed = _first_int_token(line)
        elif alt == 0 and stripped.startswith("Alt-Ergo"):
            alt = _first_int_token(line)
        if qed and alt:
            break
    return qed, alt, total


# ─── Progress bar ───────────────────────────────────────────────────────────────

def _emit_progress(current: int, total: int, label: str, rc: int,
                   qed: int, total_goals: int, elapsed: float,
                   bench_t0: float) -> None:
    width = 24
    filled = (current * width // total) if total > 0 else 0
    filled = min(filled, width)
    bar = "#" * filled + "." * (width - filled)
    pct = (current * 100 // total) if total > 0 else 0
    tag = "ok" if rc == 0 else "FAIL"
    since = int(time.time() - bench_t0)
    print(f"[bench {current:3d}/{total:3d}] [{bar}] {pct:3d}%  "
          f"{label:<44s}  {tag:<4s}  {qed}/{total_goals} goals  "
          f"{elapsed:5.2f}s  (run {since//60}m{since%60:02d}s)",
          file=sys.stderr, flush=True)


# ─── Inside-container subcommand ────────────────────────────────────────────────

def cmd_inside_run(args) -> int:
    log_dir = pathlib.Path(os.environ.get("LOG_DIR", "/tmp/wp-logs"))
    log_dir.mkdir(parents=True, exist_ok=True)
    total = len(TARGETS)
    bench_t0 = time.time()
    par = os.environ.get("WP_PAR")
    print(f"[bench] {total} targets queued "
          f"(WP_PAR={par if par is not None else 'default'})",
          file=sys.stderr, flush=True)
    for i, t in enumerate(TARGETS, 1):
        rc, elapsed = run_target(t, log_dir)
        qed, alt, total_goals = parse_log(log_dir / f"{t.label}.log")
        print(f"RESULT|{t.label}|{qed}|{alt}|{total_goals}|"
              f"{elapsed:.2f}|rc={rc}",
              flush=True)
        _emit_progress(i, total, t.label, rc, qed, total_goals, elapsed,
                       bench_t0)
    secs = int(time.time() - bench_t0)
    print(f"[bench] done — {total}/{total} targets, "
          f"total {secs//60}m{secs%60:02d}s",
          file=sys.stderr, flush=True)
    return 0


# ─── Render subcommand ──────────────────────────────────────────────────────────

def load_results(path: pathlib.Path) -> dict:
    data: dict = {}
    if not path.exists():
        return data
    for line in path.read_text().splitlines():
        if not line.startswith("RESULT|"):
            continue
        _, label, qed, alt, total, elapsed, rc = line.split("|", 6)
        data[label] = {
            "qed": int(qed), "alt": int(alt),
            "total": int(total), "elapsed": float(elapsed),
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
        qed += d["qed"]; alt += d["alt"]; total += d["total"]
        elapsed += d["elapsed"]
    return qed, alt, total, elapsed


TABULAR_PREAMBLE = (
    r"\begin{tabular}{>{\raggedright}p{0.38\linewidth}"
    r" >{\centering}p{0.05\linewidth}"
    r">{\centering}p{0.13\linewidth}"
    r">{\centering\arraybackslash}p{0.20\linewidth}}"
)


def render_one(caption: str, label: str, groups, data: dict) -> str:
    missing: set = set()
    lines: List[str] = []
    lines.append(r"\begin{table}[tp]")
    lines.append(r"    \small")
    lines.append(r"    \centering")
    lines.append(f"    \\caption{{{caption}}}")
    lines.append(f"    \\label{{{label}}}")
    lines.append(TABULAR_PREAMBLE)
    lines.append(r"    & \multicolumn{2}{c}{\textbf{Proof Goals}} & \multirow{2}{*}{\textbf{Time}}\\ \cmidrule{2-3}")
    lines.append(r"    & Qed & Alt-Ergo & \\ \hline \hline")
    tq = ta = 0
    tt = 0.0
    for display, labels in groups:
        qed, alt, _, elapsed = fold(labels, data, missing_sink=missing)
        tq += qed; ta += alt; tt += elapsed
        lines.append(f"    \\texttt{{{display}}} & {qed} & {alt} & {{$\\approx {elapsed:.1f}\\,s$}} \\\\")
    lines.append(r"    \midrule")
    lines.append(f"    \\textbf{{Total}} & \\textbf{{{tq}}} & \\textbf{{{ta}}} & {{$\\approx {tt:.1f}\\,s$}} \\\\")
    lines.append(r"\end{tabular}")
    lines.append(r"\end{table}")
    if missing:
        lines.insert(0, f"% WARNING: missing RESULT labels in source CSV: {sorted(missing)}")
    return "\n".join(lines)


def cmd_render(args) -> int:
    par = load_results(pathlib.Path(args.parallel))
    ser = load_results(pathlib.Path(args.serial))
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
        print(render_one(caption, label, groups, data))
    return 0


# ─── Host-side run subcommand ───────────────────────────────────────────────────

def _run_pass(repo_root: pathlib.Path, bench_dir: pathlib.Path,
              results_dir: pathlib.Path, image: str,
              name: str, wp_par_env: str) -> None:
    out_file = results_dir / f"run-{name}.out"
    err_file = results_dir / f"run-{name}.err"
    logs_subdir = results_dir / f"logs-{name}"
    logs_subdir.mkdir(exist_ok=True)
    print(f"[bench] === {name} pass ({wp_par_env or 'default WP_PAR'}) ===",
          file=sys.stderr, flush=True)
    cmd = [
        "docker", "run", "--rm",
        "-v", f"{repo_root}/rtems:/workspace/rtems",
        "-v", f"{repo_root}/scripts:/opt/scripts",
        "-v", f"{repo_root}/verification:/workspace/verification",
        "-v", f"{repo_root}/source:/workspace/source",
        "-v", f"{bench_dir}:/opt/bench",
        "-v", f"{logs_subdir}:/tmp/wp-logs",
        "-e", wp_par_env,
        "-e", f"WP_TIMEOUT={os.environ.get('WP_TIMEOUT', '120')}",
        image,
        "python3", "/opt/bench/bench.py", "inside-run",
    ]
    with out_file.open("wb") as outf, err_file.open("wb") as errf:
        proc = subprocess.Popen(cmd, stdout=outf, stderr=subprocess.PIPE,
                                bufsize=0)
        assert proc.stderr is not None
        for line in proc.stderr:
            sys.stderr.buffer.write(line)
            sys.stderr.buffer.flush()
            errf.write(line)
        proc.wait()
    # Pull RESULT lines into the results-<pass>.txt file.
    results = [ln for ln in out_file.read_text(errors="replace").splitlines()
               if ln.startswith("RESULT")]
    txt_path = results_dir / f"results-{name}.txt"
    txt_path.write_text("\n".join(results) + ("\n" if results else ""))
    print(f"[bench] -> {txt_path} ({len(results)} rows)",
          file=sys.stderr, flush=True)


def cmd_run(args) -> int:
    bench_dir = pathlib.Path(__file__).resolve().parent
    repo_root = bench_dir.parent.parent
    results_dir = bench_dir / "results"
    results_dir.mkdir(exist_ok=True)

    image = os.environ.get("IMAGE", "rtems-edf-toolchain-fc32")

    pass_choice = args.pass_choice or os.environ.get("PASS", "both")
    render_only = args.render_only or bool(os.environ.get("RENDER_ONLY"))

    if not render_only:
        if pass_choice == "parallel":
            _run_pass(repo_root, bench_dir, results_dir, image,
                      "parallel", "WP_PAR=")
        elif pass_choice == "serial":
            _run_pass(repo_root, bench_dir, results_dir, image,
                      "serial", "WP_PAR=1")
        elif pass_choice == "both":
            _run_pass(repo_root, bench_dir, results_dir, image,
                      "parallel", "WP_PAR=")
            _run_pass(repo_root, bench_dir, results_dir, image,
                      "serial", "WP_PAR=1")
        else:
            print(f"unknown PASS={pass_choice}", file=sys.stderr)
            return 2

    print("[bench] === LaTeX ===", file=sys.stderr, flush=True)
    return cmd_render(argparse.Namespace(
        parallel=str(results_dir / "results-parallel.txt"),
        serial=str(results_dir / "results-serial.txt"),
    ))


# ─── CLI ────────────────────────────────────────────────────────────────────────

def main() -> int:
    ap = argparse.ArgumentParser(
        description="Per-function WP benchmark (Python port).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=("Examples:\n"
                "  python3 bench.py run                  # both passes + LaTeX\n"
                "  PASS=parallel python3 bench.py run    # parallel pass only\n"
                "  RENDER_ONLY=1 python3 bench.py run    # skip docker, just render\n"
                "  python3 bench.py inside-run           # container worker\n"))
    sub = ap.add_subparsers(dest="cmd", required=True)

    p_run = sub.add_parser("run", help="host-side: docker + render")
    p_run.add_argument("--pass", dest="pass_choice", default=None,
                       choices=["parallel", "serial", "both"],
                       help="which pass to run (default: $PASS or 'both')")
    p_run.add_argument("--render-only", action="store_true",
                       help="skip docker, just render existing results")
    p_run.set_defaults(func=cmd_run)

    p_in = sub.add_parser("inside-run",
                          help="container-side worker (iterates TARGETS)")
    p_in.set_defaults(func=cmd_inside_run)

    p_r = sub.add_parser("render",
                         help="re-render previously gathered results")
    bench_dir = pathlib.Path(__file__).resolve().parent
    p_r.add_argument("--parallel",
                     default=str(bench_dir / "results" / "results-parallel.txt"))
    p_r.add_argument("--serial",
                     default=str(bench_dir / "results" / "results-serial.txt"))
    p_r.set_defaults(func=cmd_render)

    args = ap.parse_args()
    return args.func(args) or 0


if __name__ == "__main__":
    sys.exit(main())
