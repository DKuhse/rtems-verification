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
import fnmatch
import os
import pathlib
import re
import shutil
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
                  script=f"/opt/scripts/5.1/{script_base}", fct=fct)

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
    _62("scheduler_cancel_job",      "verify-scheduler-cancel-job.sh",    "_Scheduler_Cancel_job"),
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
    """Build the per-invocation flags appended to every frama-c call.

    -wp-par defaults to `nproc` (frama-c's own default is 4, which leaves a
    lot of cores on the table for the parallel pass). The serial pass
    explicitly sets WP_PAR=1; users can pass any other value to override.
    """
    par = os.environ.get("WP_PAR") or str(os.cpu_count() or 4)
    return ["-wp-timeout", WP_TIMEOUT, "-wp-par", par]


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
    # _wp_extra() appends -wp-timeout / -wp-par; the verify script's own
    # defaults are overridden via Frama-C's last-wins.
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


def parse_log(log_path: pathlib.Path) -> Tuple[int, int, int, int]:
    """Return (qed, alt_ergo, proved, total) from the function-pass section.

    `proved` and `total` come from Frama-C's `[wp] Proved goals: X / Y`
    summary line: X is the number of goals proved by **any** prover, Y is
    the total. Qed and Alt-Ergo numbers are individual prover counts
    (typically proved == qed + alt). The bench checks `proved == total`
    to decide whether a target passed, rather than relying on the shell
    exit code — Frama-C doesn't return non-zero just because a goal timed
    out under Alt-Ergo, so on slow hardware `rc==0` is insufficient.

    When the script emits `=== ... function ===` / `=== ... model lemma ===`
    markers (the EDF scripts do, for the lemma pass that's shared across rows
    and shouldn't be double-counted), only the function section is parsed.
    Otherwise the whole log is parsed.
    """
    try:
        text = log_path.read_text(errors="replace")
    except FileNotFoundError:
        return (0, 0, 0, 0)
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

    proved = total = qed = alt = 0
    fnpass = "\n".join(fnpass_lines)
    if (m := _RE_PROVED.search(fnpass)):
        proved = int(m.group(1))
        total  = int(m.group(2))
    for line in fnpass_lines:
        stripped = line.lstrip()
        if qed == 0 and stripped.startswith("Qed:"):
            qed = _first_int_token(line)
        elif alt == 0 and stripped.startswith("Alt-Ergo"):
            alt = _first_int_token(line)
        if qed and alt:
            break
    return qed, alt, proved, total


# ─── Progress bar ───────────────────────────────────────────────────────────────

def _target_system(t: Target) -> str:
    """Short tag identifying which system a target belongs to.

    Used purely for the live progress bar — RESULT lines and rendered
    tables are unchanged. Derived from the driver + script path so we
    don't have to annotate every TARGETS entry.
    """
    if t.driver == "uni_helper":
        return "6.2"
    if t.driver == "heir51_helper":
        return "5.1"
    if t.driver == "freertos":
        return "FR"
    if t.driver == "script" and t.script:
        if "/scripts/6.2/" in t.script:
            return "6.2"
        if "/scripts/5.1/" in t.script:
            return "5.1"
    return "???"


def _emit_progress(current: int, total: int, system: str, label: str, rc: int,
                   proved: int, total_goals: int, elapsed: float,
                   bench_t0: float) -> None:
    width = 24
    filled = (current * width // total) if total > 0 else 0
    filled = min(filled, width)
    bar = "#" * filled + "." * (width - filled)
    pct = (current * 100 // total) if total > 0 else 0
    # "ok" requires both clean exit AND every goal proved by some prover —
    # frama-c won't fail on its own when individual goals time out.
    tag = "ok" if (rc == 0 and proved == total_goals) else "FAIL"
    sys_tag = f"[{system}]"
    since = int(time.time() - bench_t0)
    print(f"[bench {current:3d}/{total:3d}] [{bar}] {pct:3d}%  "
          f"{sys_tag:<5s} {label:<40s}  {tag:<4s}  "
          f"{proved}/{total_goals} goals  "
          f"{elapsed:5.2f}s  (run {since//60}m{since%60:02d}s)",
          file=sys.stderr, flush=True)


# ─── Inside-container subcommand ────────────────────────────────────────────────

def _load_opam_env() -> None:
    """Mirror the bash `eval $(opam env)` so `frama-c` lands on PATH.

    The verify-*.sh scripts and `inside-container.sh` do this at the top
    because `frama-c` lives in the opam switch's bin dir, not in
    /usr/bin. Without it, subprocess.run("frama-c", ...) raises
    FileNotFoundError. We shell out to sh so it handles opam's quoting
    rules itself, then mirror the resulting env into os.environ.
    """
    if not shutil.which("opam"):
        return
    try:
        out = subprocess.check_output(
            ["sh", "-c", "eval $(opam env) && env -0"],
            stderr=subprocess.DEVNULL,
        )
    except (subprocess.CalledProcessError, FileNotFoundError):
        return
    for entry in out.split(b"\0"):
        if not entry:
            continue
        name, _, value = entry.partition(b"=")
        try:
            os.environ[name.decode("utf-8")] = value.decode("utf-8")
        except UnicodeDecodeError:
            continue


def _expand_patterns(raw: List[str]) -> List[str]:
    """Flatten comma-separated `--target` args. `--target a,b --target c`
    becomes `["a", "b", "c"]`."""
    out: List[str] = []
    for entry in raw:
        out.extend(p.strip() for p in entry.split(",") if p.strip())
    return out


def _filter_targets(patterns: List[str]) -> List[Target]:
    """Return TARGETS filtered by fnmatch against `patterns`.

    Multiple patterns are OR'd. Order matches the TARGETS list (so the
    progress bar walks them in their original order, not pattern order).
    A pattern that matches nothing prints a warning but doesn't abort.
    """
    if not patterns:
        return list(TARGETS)
    matched_for = {pat: [t for t in TARGETS if fnmatch.fnmatch(t.label, pat)]
                   for pat in patterns}
    for pat, hits in matched_for.items():
        if not hits:
            print(f"[bench] warning: pattern {pat!r} matched no targets",
                  file=sys.stderr)
    keep_labels: set = set()
    for hits in matched_for.values():
        for t in hits:
            keep_labels.add(t.label)
    return [t for t in TARGETS if t.label in keep_labels]


def _resolve_target_patterns(args) -> List[str]:
    """Patterns from --target (cli) win, else BENCH_TARGETS (env), else []."""
    raw = list(getattr(args, "target", None) or [])
    if not raw and os.environ.get("BENCH_TARGETS"):
        raw = [os.environ["BENCH_TARGETS"]]
    return _expand_patterns(raw)


def _load_sidecar(path: pathlib.Path) -> dict:
    """Read previously-completed RESULT lines into {label: raw_line}.

    The sidecar lives inside LOG_DIR (which is mounted from the host) so
    it persists across docker invocations and Ctrl-C kills.
    """
    out: dict = {}
    if not path.exists():
        return out
    for line in path.read_text().splitlines():
        if not line.startswith("RESULT|"):
            continue
        try:
            label = line.split("|", 2)[1]
        except IndexError:
            continue
        out[label] = line
    return out


def cmd_inside_run(args) -> int:
    _load_opam_env()
    log_dir = pathlib.Path(os.environ.get("LOG_DIR", "/tmp/wp-logs"))
    log_dir.mkdir(parents=True, exist_ok=True)
    patterns = _resolve_target_patterns(args)
    targets = _filter_targets(patterns)
    total = len(targets)
    bench_t0 = time.time()

    resume = bool(getattr(args, "resume", False)) or bool(os.environ.get("RESUME"))
    sidecar_path = log_dir / "results.txt"
    cached: dict = _load_sidecar(sidecar_path) if resume else {}

    # When resuming, truncate-rewrite the sidecar to only contain entries we
    # still consider valid (i.e. labels currently being filtered AND already
    # cached). Anything else gets re-run and re-appended below. Without this
    # the sidecar would grow stale entries across re-runs with different
    # --target filters.
    mode = "w"  # always start with a known-good file
    sidecar_f = open(sidecar_path, mode, buffering=1)  # line-buffered
    try:
        for t in targets:
            if t.label in cached:
                sidecar_f.write(cached[t.label] + "\n")

        sel_note = f", filter={patterns}" if patterns else ""
        skip_count = sum(1 for t in targets if t.label in cached)
        resume_note = f", resume=skipping {skip_count}" if resume and skip_count else ""
        print(f"[bench] {total}/{len(TARGETS)} targets queued "
              f"({' '.join(_wp_extra())}{sel_note}{resume_note})",
              file=sys.stderr, flush=True)
        if not targets:
            print("[bench] nothing to run", file=sys.stderr, flush=True)
            return 0

        for i, t in enumerate(targets, 1):
            if t.label in cached:
                # Re-emit the cached RESULT verbatim so the host's grep still
                # sees it on this pass. Re-parse the log for proved/total
                # since the RESULT line only stores qed/alt/total.
                line = cached[t.label]
                print(line, flush=True)
                _, _, qed_s, alt_s, total_s, elapsed_s, rc_s = line.split("|", 6)
                rc_val = int(rc_s.removeprefix("rc=").strip())
                _, _, proved, _ = parse_log(log_dir / f"{t.label}.log")
                _emit_progress(i, total, _target_system(t), t.label, rc_val,
                               proved, int(total_s), float(elapsed_s),
                               bench_t0)
                continue
            rc, elapsed = run_target(t, log_dir)
            qed, alt, proved, total_goals = parse_log(log_dir / f"{t.label}.log")
            line = (f"RESULT|{t.label}|{qed}|{alt}|{total_goals}|"
                    f"{elapsed:.2f}|rc={rc}")
            print(line, flush=True)
            sidecar_f.write(line + "\n")
            _emit_progress(i, total, _target_system(t), t.label, rc, proved,
                           total_goals, elapsed, bench_t0)
    finally:
        sidecar_f.close()

    secs = int(time.time() - bench_t0)
    print(f"[bench] done — {total}/{total} targets, "
          f"total {secs//60}m{secs%60:02d}s",
          file=sys.stderr, flush=True)
    return 0


# ─── Render subcommand ──────────────────────────────────────────────────────────

def _prefer_sidecar(path: pathlib.Path) -> pathlib.Path:
    """If `results-<pass>.txt` is missing or empty, fall back to the
    per-pass sidecar `logs-<pass>/results.txt`, which is line-flushed and
    always reflects the latest completed targets — including from a run
    that was Ctrl-C'd before the canonical file got promoted.
    """
    if path.exists() and path.stat().st_size > 0:
        return path
    name = path.stem  # e.g. "results-parallel"
    if name.startswith("results-"):
        pass_name = name[len("results-"):]
        sidecar = path.parent / f"logs-{pass_name}" / "results.txt"
        if sidecar.exists() and sidecar.stat().st_size > 0:
            print(f"[bench] (reading {sidecar} — canonical file missing/empty)",
                  file=sys.stderr)
            return sidecar
    return path


def load_results(path: pathlib.Path) -> dict:
    data: dict = {}
    path = _prefer_sidecar(path)
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
              name: str, wp_par_env: str,
              target_patterns: List[str],
              resume: bool) -> None:
    out_file = results_dir / f"run-{name}.out"
    err_file = results_dir / f"run-{name}.err"
    logs_subdir = results_dir / f"logs-{name}"
    logs_subdir.mkdir(exist_ok=True)
    parts = []
    if target_patterns:
        parts.append(f"targets={target_patterns}")
    if resume:
        parts.append("resume=on")
    note = (", " + ", ".join(parts)) if parts else ""
    print(f"[bench] === {name} pass ({wp_par_env or 'nproc'}{note}) ===",
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
    ]
    if target_patterns:
        cmd += ["-e", f"BENCH_TARGETS={','.join(target_patterns)}"]
    if resume:
        cmd += ["-e", "RESUME=1"]
    cmd += [
        image,
        "python3", "/opt/bench/bench.py", "inside-run",
    ]
    try:
        with out_file.open("wb") as outf, err_file.open("wb") as errf:
            proc = subprocess.Popen(cmd, stdout=outf, stderr=subprocess.PIPE,
                                    bufsize=0)
            assert proc.stderr is not None
            try:
                for line in proc.stderr:
                    sys.stderr.buffer.write(line)
                    sys.stderr.buffer.flush()
                    errf.write(line)
                proc.wait()
            except KeyboardInterrupt:
                # Let docker tear down the container, then fall through to
                # promote whatever the sidecar captured before SIGINT.
                proc.terminate()
                proc.wait()
                raise
    finally:
        # Promote the line-flushed sidecar to the canonical results file so
        # interrupted runs still leave a usable file. If the sidecar is
        # missing (docker died before any target completed), fall back to
        # grepping docker stdout.
        sidecar = logs_subdir / "results.txt"
        txt_path = results_dir / f"results-{name}.txt"
        if sidecar.exists() and sidecar.stat().st_size > 0:
            txt_path.write_bytes(sidecar.read_bytes())
            n = sum(1 for ln in txt_path.read_text(errors="replace").splitlines()
                    if ln.startswith("RESULT"))
            print(f"[bench] -> {txt_path} ({n} rows, from sidecar)",
                  file=sys.stderr, flush=True)
        elif out_file.exists():
            results = [ln for ln in out_file.read_text(errors="replace").splitlines()
                       if ln.startswith("RESULT")]
            txt_path.write_text("\n".join(results) + ("\n" if results else ""))
            print(f"[bench] -> {txt_path} ({len(results)} rows, from stdout)",
                  file=sys.stderr, flush=True)


def cmd_run(args) -> int:
    bench_dir = pathlib.Path(__file__).resolve().parent
    repo_root = bench_dir.parent.parent
    results_dir = bench_dir / "results"
    results_dir.mkdir(exist_ok=True)

    image = os.environ.get("IMAGE", "rtems-edf-toolchain-fc32")

    pass_choice = args.pass_choice or os.environ.get("PASS", "parallel")
    render_only = args.render_only or bool(os.environ.get("RENDER_ONLY"))
    target_patterns = _resolve_target_patterns(args)
    resume = bool(getattr(args, "resume", False)) or bool(os.environ.get("RESUME"))

    if not render_only:
        if pass_choice == "parallel":
            _run_pass(repo_root, bench_dir, results_dir, image,
                      "parallel", "WP_PAR=", target_patterns, resume)
        elif pass_choice == "serial":
            _run_pass(repo_root, bench_dir, results_dir, image,
                      "serial", "WP_PAR=1", target_patterns, resume)
        elif pass_choice == "both":
            _run_pass(repo_root, bench_dir, results_dir, image,
                      "parallel", "WP_PAR=", target_patterns, resume)
            _run_pass(repo_root, bench_dir, results_dir, image,
                      "serial", "WP_PAR=1", target_patterns, resume)
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
                "  python3 bench.py run                  # parallel pass + LaTeX\n"
                "  python3 bench.py run --pass both      # parallel + serial + LaTeX\n"
                "  PASS=parallel python3 bench.py run    # parallel pass only\n"
                "  RENDER_ONLY=1 python3 bench.py run    # skip docker, just render\n"
                "  python3 bench.py inside-run           # container worker\n"))
    sub = ap.add_subparsers(dest="cmd", required=True)

    target_help = (
        "label or fnmatch glob to run (default: all). "
        "Repeat the flag, or pass comma-separated values, to add patterns. "
        "Also reads $BENCH_TARGETS as a comma-separated fallback. "
        "Examples: --target edf_block / --target '51_edf_*' / "
        "--target edf_block,edf_unblock")

    p_run = sub.add_parser("run", help="host-side: docker + render")
    p_run.add_argument("--pass", dest="pass_choice", default=None,
                       choices=["parallel", "serial", "both"],
                       help="which pass to run (default: $PASS or 'parallel'). "
                            "'parallel' sets -wp-par to nproc; 'serial' sets "
                            "-wp-par 1; 'both' runs them sequentially. "
                            "Override with WP_PAR=N if needed.")
    p_run.add_argument("--target", action="append", default=[],
                       metavar="GLOB", help=target_help)
    p_run.add_argument("--resume", action="store_true",
                       help="skip targets already in $LOG_DIR/results.txt "
                            "(useful after Ctrl-C). Also reads $RESUME=1.")
    p_run.add_argument("--render-only", action="store_true",
                       help="skip docker, just render existing results")
    p_run.set_defaults(func=cmd_run)

    p_in = sub.add_parser("inside-run",
                          help="container-side worker (iterates TARGETS)")
    p_in.add_argument("--target", action="append", default=[],
                      metavar="GLOB", help=target_help)
    p_in.add_argument("--resume", action="store_true",
                      help="skip targets already in $LOG_DIR/results.txt")
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
