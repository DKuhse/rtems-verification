#!/bin/bash
# Runs inside the FC32 verification container. For each target (top-level
# entry point or helper):
#   - selects a single function to attribute goals to
#   - runs with -wp-cache none (clean wall time)
#   - optionally appends -wp-par $WP_PAR
#   - parses the function-pass [wp] Proved goals summary
#
# Output: one
#   RESULT|<label>|<qed>|<alt-ergo>|<total>|<elapsed-seconds>|rc=<exit-code>
# line per target on stdout, plus a one-line progress message per target on
# stderr (a textual progress bar + N/total + per-target outcome).
#
# Per-target Frama-C logs land in /tmp/wp-logs/.
#
# Env vars:
#   WP_PAR        when set, appends "-wp-par $WP_PAR" to every invocation
#   LOG_DIR       override /tmp/wp-logs
#
# Drivers — separate helpers because the underlying verify-*.sh scripts
# handle per-function selection differently:
#   - run_one(...)             — for scripts that honour WP_FCTS
#                                (most 6.2 + 5.1 scripts)
#   - run_uni_helper(...)      — direct frama-c against the 6.2
#                                scheduleruni-unblock harness
#                                (its verify-script hard-codes -wp-fct)
#   - run_51_heir_helper(...)  — direct frama-c against the 5.1
#                                scheduler-update-heir harness
#                                (its verify-script hard-codes -wp-fct)
#   - run_freertos_one(...)    — direct frama-c against the FreeRTOS
#                                reference slices (their verify-scripts
#                                hard-code -wp-fct)
#
# `-wp-split` is baked into verify-edf-release-cancel.sh's WP_FCT_DEFAULTS
# in both 6.2 and 5.1, so it survives the WP_FCTS override and applies to
# every run_one call that targets those scripts.
#
# WP_TIMEOUT (default 120) — per-goal prover timeout in seconds. The verify
# scripts themselves use 30s; this artifact runner uses a generous default so
# the bench doesn't misfire on slower reviewer hardware. The bench appends a
# fresh -wp-timeout to every invocation; Frama-C uses the last value seen.
set -u

LOG_DIR="${LOG_DIR:-/tmp/wp-logs}"
mkdir -p "${LOG_DIR}"

WP_TIMEOUT="${WP_TIMEOUT:-120}"

EXTRA_ARGS=(-wp-timeout "${WP_TIMEOUT}")
if [ -n "${WP_PAR:-}" ]; then
    EXTRA_ARGS+=(-wp-par "${WP_PAR}")
fi

if command -v opam >/dev/null 2>&1; then
    eval "$(opam env)"
fi

# Shared paths for the direct-frama-c helpers below.
RTEMS_SRC_62="${RTEMS_SRC_62:-/workspace/rtems/src/rtems-6.2-pristine}"
RTEMS_SRC_51="${RTEMS_SRC_51:-/workspace/rtems/src/rtems-5.1-pristine}"
RTEMS_PREFIX="${RTEMS_PREFIX:-/opt/rtems5}"
OVERLAY_62="${OVERLAY_62:-/workspace/verification/6.2}"
OVERLAY_51="${OVERLAY_51:-/workspace/verification/5.1}"
RTEMS_BUILD_BSP="${RTEMS_BUILD_BSP:-/workspace/rtems/build/amd64/x86_64-rtems5/c/amd64/include}"

# ── Progress bar ────────────────────────────────────────────────────────────────
# TOTAL_TARGETS is computed by counting `run_*` call sites in this very file
# (only lines that start at the left margin, which is how the call sites are
# laid out below). Drop, add, or reorder calls freely.
TOTAL_TARGETS=$(grep -cE '^(run_one|run_uni_helper|run_51_heir_helper|run_freertos_one) ' "${BASH_SOURCE[0]}")
CURRENT_TARGET=0
BENCH_T0=$(date +%s)

_format_bar() {
    local current="$1" total="$2" width="$3"
    local filled empty
    if [ "${total}" -le 0 ]; then
        filled=0
    else
        filled=$((current * width / total))
    fi
    [ "${filled}" -gt "${width}" ] && filled=${width}
    empty=$((width - filled))
    printf '%s%s' \
        "$(printf '%*s' "${filled}" '' | tr ' ' '#')" \
        "$(printf '%*s' "${empty}"  '' | tr ' ' '.')"
}

_print_progress() {
    local label="$1" rc="$2" qed="$3" total="$4" elapsed="$5"
    local pct=0
    if [ "${TOTAL_TARGETS}" -gt 0 ]; then
        pct=$((CURRENT_TARGET * 100 / TOTAL_TARGETS))
    fi
    local bar
    bar=$(_format_bar "${CURRENT_TARGET}" "${TOTAL_TARGETS}" 24)
    local tag="ok"
    [ "${rc}" != "0" ] && tag="FAIL"
    local now=$(date +%s)
    local since=$((now - BENCH_T0))
    printf '[bench %3d/%3d] [%s] %3d%%  %-44s  %-4s  %s/%s goals  %5ss  (run %dm%02ds)\n' \
        "${CURRENT_TARGET}" "${TOTAL_TARGETS}" "${bar}" "${pct}" \
        "${label}" "${tag}" "${qed}" "${total}" "${elapsed}" \
        "$((since/60))" "$((since%60))" >&2
}

# Parses /tmp/wp-logs/${label}.log and emits the RESULT line + progress line.
emit_result() {
    local label="$1" elapsed="$2" rc="$3"
    local log="${LOG_DIR}/${label}.log"

    awk '
        /=== .* function ===/ { in_fn=1; next }
        /=== .* model lemma ===/ { in_fn=0 }
        in_fn { print }
    ' "${log}" > "${log}.fnpass"
    if [ ! -s "${log}.fnpass" ]; then
        cp "${log}" "${log}.fnpass"
    fi

    total=$(awk '/^\[wp\] Proved goals:/{ gsub(/[^0-9]/," ",$0); print $2; exit }' "${log}.fnpass")
    qed=$(awk '/^  Qed:/{ for(i=1;i<=NF;i++) if($i ~ /^[0-9]+$/){print $i; exit} }' "${log}.fnpass")
    alt=$(awk '/^  Alt-Ergo/{ for(i=1;i<=NF;i++) if($i ~ /^[0-9]+$/){print $i; exit} }' "${log}.fnpass")
    : "${total:=0}" "${qed:=0}" "${alt:=0}" "${elapsed:=0}"
    echo "RESULT|${label}|${qed}|${alt}|${total}|${elapsed}|rc=${rc}"

    CURRENT_TARGET=$((CURRENT_TARGET + 1))
    _print_progress "${label}" "${rc}" "${qed}" "${total}" "${elapsed}"
}

run_one() {
    local label="$1" script="$2" fcts="$3"
    local log="${LOG_DIR}/${label}.log"
    local t0 t1
    t0=$(date +%s.%N)
    env WP_FCTS="${fcts}" "${script}" -wp-cache none "${EXTRA_ARGS[@]}" >"${log}" 2>&1
    local rc=$?
    t1=$(date +%s.%N)
    emit_result "${label}" "$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.2f", b-a}')" "${rc}"
}

# Direct invocation against the 6.2 scheduleruni-unblock harness (verify-script
# hard-codes -wp-fct, so we can't ride WP_FCTS).
run_uni_helper() {
    local label="$1" fct="$2"
    local log="${LOG_DIR}/${label}.log"
    local SRC="${OVERLAY_62}/harnesses/scheduleruni-unblock-harness.c"

    local CPP_CMD="${RTEMS_PREFIX}/bin/x86_64-rtems5-gcc -C -E \
        -D__FRAMAC__ -D__rtems__ \
        -I${OVERLAY_62}/overlay/cpukit/include \
        -I${OVERLAY_62}/models \
        -I${RTEMS_SRC_62}/cpukit/include \
        -I${RTEMS_SRC_62}/cpukit/score/cpu/x86_64/include \
        -I${RTEMS_BUILD_BSP} \
        -I${RTEMS_PREFIX}/x86_64-rtems5/include \
        -I${RTEMS_PREFIX}/lib/gcc/x86_64-rtems5/9.3.0/include \
        -I${RTEMS_SRC_62}/bsps/include \
        -I${RTEMS_SRC_62}/bsps/x86_64/include \
        -I${RTEMS_SRC_62}/bsps/x86_64/amd64/include \
        -nostdinc"

    local t0 t1
    t0=$(date +%s.%N)
    frama-c \
        -cpp-command "${CPP_CMD}" \
        -machdep gcc_x86_64 -cpp-frama-c-compliant -std c11 \
        "${SRC}" \
        -volatile -then-on Volatile \
        -wp -wp-fct "${fct}" -wp-model "Typed+Cast" \
        -wp-cache none \
        "${EXTRA_ARGS[@]}" \
        >"${log}" 2>&1
    local rc=$?
    t1=$(date +%s.%N)
    emit_result "${label}" "$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.2f", b-a}')" "${rc}"
}

# Direct invocation against the 5.1 scheduler-update-heir harness (its
# verify-script hard-codes -wp-fct against this harness translation unit).
run_51_heir_helper() {
    local label="$1" fct="$2"
    local log="${LOG_DIR}/${label}.log"
    local SRC="${OVERLAY_51}/harnesses/scheduler-update-heir-harness.c"

    local CPP_CMD="${RTEMS_PREFIX}/bin/x86_64-rtems5-gcc -C -E \
        -D__FRAMAC__ -D__rtems__ \
        -I${OVERLAY_51}/overlay/cpukit/include \
        -I${OVERLAY_51}/models \
        -I${RTEMS_SRC_51}/cpukit/include \
        -I${RTEMS_SRC_51}/cpukit/score/cpu/x86_64/include \
        -I${RTEMS_BUILD_BSP} \
        -I${RTEMS_PREFIX}/x86_64-rtems5/include \
        -I${RTEMS_PREFIX}/lib/gcc/x86_64-rtems5/9.3.0/include \
        -I${RTEMS_SRC_51}/bsps/include \
        -I${RTEMS_SRC_51}/bsps/x86_64/include \
        -I${RTEMS_SRC_51}/bsps/x86_64/amd64/include \
        -nostdinc"

    local t0 t1
    t0=$(date +%s.%N)
    frama-c \
        -cpp-command "${CPP_CMD}" \
        -machdep gcc_x86_64 -cpp-frama-c-compliant -std c11 \
        "${SRC}" \
        -volatile -then-on Volatile \
        -wp -wp-fct "${fct}" -wp-model "Typed+Cast" \
        -wp-cache none \
        "${EXTRA_ARGS[@]}" \
        >"${log}" 2>&1
    local rc=$?
    t1=$(date +%s.%N)
    emit_result "${label}" "$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.2f", b-a}')" "${rc}"
}

# Direct invocation against the FreeRTOS reference slices.
run_freertos_one() {
    local label="$1" source_rel="$2" fct="$3"
    local log="${LOG_DIR}/${label}.log"
    local OVERLAY=/workspace/verification/freertos
    local FREERTOS_SRC=/workspace/source/freertos-edf-msp430

    local CPP_CMD="gcc -C -E \
        -D__LARGE_DATA_MODEL__ -D__FRAMAC__ -DEDF_SCHEDULER=1 \
        -I${OVERLAY}/model \
        -I${OVERLAY}/overlay/include \
        -I${OVERLAY}/stubs \
        -I${FREERTOS_SRC}/include \
        -nostdinc \
        -isystem /usr/include \
        -isystem /usr/include/x86_64-linux-gnu \
        -isystem /usr/lib/gcc/x86_64-linux-gnu/11/include"

    local t0 t1
    t0=$(date +%s.%N)
    frama-c \
        -cpp-command "${CPP_CMD}" \
        -machdep gcc_x86_16 -cpp-frama-c-compliant -std c11 \
        "${OVERLAY}/${source_rel}" \
        -volatile -then-on Volatile \
        -wp -wp-fct "${fct}" -wp-model "Typed+Cast" \
        -wp-cache none \
        "${EXTRA_ARGS[@]}" \
        >"${log}" 2>&1
    local rc=$?
    t1=$(date +%s.%N)
    emit_result "${label}" "$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.2f", b-a}')" "${rc}"
}

printf '[bench] %d targets queued (WP_PAR=%s)\n' "${TOTAL_TARGETS}" "${WP_PAR:-default}" >&2

# ── RTEMS 6.2 top-level entry points ────────────────────────────────────────────
run_one "edf_initialize"             /opt/scripts/6.2/verify-edf-initialize.sh         "_Scheduler_EDF_Initialize"
run_one "edf_node_initialize"        /opt/scripts/6.2/verify-edf-node-initialize.sh    "_Scheduler_EDF_Node_initialize"
run_one "edf_block"                  /opt/scripts/6.2/verify-edf-block.sh              "_Scheduler_EDF_Block"
run_one "edf_schedule"               /opt/scripts/6.2/verify-edf-schedule.sh           "_Scheduler_EDF_Schedule"
run_one "edf_yield"                  /opt/scripts/6.2/verify-edf-yield.sh              "_Scheduler_EDF_Yield"
run_one "edf_unblock"                /opt/scripts/6.2/verify-edf-unblock.sh            "_Scheduler_EDF_Unblock"
run_one "edf_update_priority"        /opt/scripts/6.2/verify-edf-update-priority.sh    "_Scheduler_EDF_Update_priority"
run_one "edf_release_job"            /opt/scripts/6.2/verify-edf-release-cancel.sh     "_Scheduler_EDF_Release_job"
run_one "edf_cancel_job"             /opt/scripts/6.2/verify-edf-release-cancel.sh     "_Scheduler_EDF_Cancel_job"
# Uniprocessor Unblock — verify-scheduleruni-unblock.sh hard-codes 4 fcts,
# so we drive it directly to isolate just _Scheduler_uniprocessor_Unblock.
run_uni_helper "scheduleruni_unblock"  "_Scheduler_uniprocessor_Unblock"
run_one "scheduler_release_job"      /opt/scripts/6.2/verify-scheduler-release-job.sh    "_Scheduler_Release_job"
run_one "scheduler_update_priority"  /opt/scripts/6.2/verify-scheduler-update-priority.sh "_Scheduler_Update_priority"
run_one "thread_priority_add"        /opt/scripts/6.2/verify-thread-change-priority.sh "_Thread_Priority_add"
run_one "thread_priority_changed"    /opt/scripts/6.2/verify-thread-change-priority.sh "_Thread_Priority_changed"
run_one "thread_priority_remove"     /opt/scripts/6.2/verify-thread-change-priority.sh "_Thread_Priority_remove"
run_one "thread_priority_update"     /opt/scripts/6.2/verify-thread-priority-update.sh "_Thread_Priority_update"
run_one "ratemon_release_job"        /opt/scripts/6.2/verify-ratemon-release-job.sh    "_Rate_monotonic_Release_job"
run_one "ratemon_cancel"             /opt/scripts/6.2/verify-ratemon-cancel.sh         "_Rate_monotonic_Cancel"

# ── RTEMS 6.2 EDF scheduler helpers (each verified once) ────────────────────────
run_one "h_edf_get_context"           /opt/scripts/6.2/verify-edf-block.sh           "_Scheduler_EDF_Get_context"
run_one "h_edf_node_downcast"         /opt/scripts/6.2/verify-edf-block.sh           "_Scheduler_EDF_Node_downcast"
run_one "h_edf_map_priority"          /opt/scripts/6.2/verify-edf-release-cancel.sh  "_Scheduler_EDF_Map_priority"
run_one "h_edf_unmap_priority"        /opt/scripts/6.2/verify-edf-release-cancel.sh  "_Scheduler_EDF_Unmap_priority"
run_one "h_scheduler_get_context"     /opt/scripts/6.2/verify-edf-initialize.sh      "_Scheduler_Get_context"
run_one "h_rbtree_init_empty"         /opt/scripts/6.2/verify-edf-initialize.sh      "_RBTree_Initialize_empty"
run_one "h_node_do_initialize"        /opt/scripts/6.2/verify-edf-node-initialize.sh "_Scheduler_Node_do_initialize"
run_one "h_rbtree_init_node"          /opt/scripts/6.2/verify-edf-node-initialize.sh "_RBTree_Initialize_node"
run_one "h_thread_is_ready"           /opt/scripts/6.2/verify-edf-schedule.sh        "_Thread_Is_ready"
run_uni_helper "h_uni_update_heir"                "_Scheduler_uniprocessor_Update_heir"
run_uni_helper "h_uni_update_heir_if_necessary"   "_Scheduler_uniprocessor_Update_heir_if_necessary"
run_uni_helper "h_uni_update_heir_if_preemptible" "_Scheduler_uniprocessor_Update_heir_if_preemptible"

# ── RTEMS 6.2 priority helpers (verify-thread-change-priority.sh) ───────────────
run_one "h_priority_actions_add"           /opt/scripts/6.2/verify-thread-change-priority.sh "_Priority_Actions_add"
run_one "h_priority_non_empty_insert"      /opt/scripts/6.2/verify-thread-change-priority.sh "_Priority_Non_empty_insert"
run_one "h_priority_extract_non_empty"     /opt/scripts/6.2/verify-thread-change-priority.sh "_Priority_Extract_non_empty"
run_one "h_priority_changed"               /opt/scripts/6.2/verify-thread-change-priority.sh "_Priority_Changed"
run_one "h_thread_set_sched_node_prio"     /opt/scripts/6.2/verify-thread-change-priority.sh "_Thread_Set_scheduler_node_priority"
run_one "h_thread_priority_action_change"  /opt/scripts/6.2/verify-thread-change-priority.sh "_Thread_Priority_action_change"
run_one "h_thread_queue_do_nothing_pa"     /opt/scripts/6.2/verify-thread-change-priority.sh "_Thread_queue_Do_nothing_priority_actions"
run_one "h_thread_priority_do_perform"     /opt/scripts/6.2/verify-thread-change-priority.sh "_Thread_Priority_do_perform_actions"
run_one "h_thread_priority_apply"          /opt/scripts/6.2/verify-thread-change-priority.sh "_Thread_Priority_apply"
run_one "h_scheduler_node_set_priority"    /opt/scripts/6.2/verify-thread-change-priority.sh "_Scheduler_Node_set_priority"

# ── RTEMS 5.1 top-level entry points ────────────────────────────────────────────
# 5.1 doesn't split unblock into uniprocessor/EDF parts the way 6.2 does;
# the standalone uniprocessor helper is `_Scheduler_Update_heir`, verified
# via its own harness (run_51_heir_helper).
run_one "51_edf_initialize"             /opt/scripts/5.1/verify-edf-initialize.sh         "_Scheduler_EDF_Initialize"
run_one "51_edf_node_initialize"        /opt/scripts/5.1/verify-edf-node-initialize.sh    "_Scheduler_EDF_Node_initialize"
run_one "51_edf_block"                  /opt/scripts/5.1/verify-edf-block.sh              "_Scheduler_EDF_Block"
run_one "51_edf_schedule"               /opt/scripts/5.1/verify-edf-schedule.sh           "_Scheduler_EDF_Schedule"
run_one "51_edf_yield"                  /opt/scripts/5.1/verify-edf-yield.sh              "_Scheduler_EDF_Yield"
run_one "51_edf_unblock"                /opt/scripts/5.1/verify-edf-unblock.sh            "_Scheduler_EDF_Unblock"
run_one "51_edf_update_priority"        /opt/scripts/5.1/verify-edf-update-priority.sh    "_Scheduler_EDF_Update_priority"
run_one "51_edf_release_job"            /opt/scripts/5.1/verify-edf-release-cancel.sh     "_Scheduler_EDF_Release_job"
run_one "51_edf_cancel_job"             /opt/scripts/5.1/verify-edf-release-cancel.sh     "_Scheduler_EDF_Cancel_job"
run_51_heir_helper "51_scheduler_update_heir"  "_Scheduler_Update_heir"
run_one "51_scheduler_release_job"      /opt/scripts/5.1/verify-scheduler-release-job.sh    "_Scheduler_Release_job"
run_one "51_scheduler_cancel_job"       /opt/scripts/5.1/verify-scheduler-cancel-job.sh     "_Scheduler_Cancel_job"
run_one "51_scheduler_update_priority"  /opt/scripts/5.1/verify-scheduler-update-priority.sh "_Scheduler_Update_priority"
run_one "51_thread_priority_add"        /opt/scripts/5.1/verify-thread-change-priority.sh "_Thread_Priority_add"
run_one "51_thread_priority_changed"    /opt/scripts/5.1/verify-thread-change-priority.sh "_Thread_Priority_changed"
run_one "51_thread_priority_remove"     /opt/scripts/5.1/verify-thread-change-priority.sh "_Thread_Priority_remove"
run_one "51_thread_priority_update"     /opt/scripts/5.1/verify-thread-priority-update.sh "_Thread_Priority_update"
run_one "51_ratemon_release_job"        /opt/scripts/5.1/verify-ratemon-release-job.sh    "_Rate_monotonic_Release_job"
run_one "51_ratemon_cancel"             /opt/scripts/5.1/verify-ratemon-cancel.sh         "_Rate_monotonic_Cancel"

# ── RTEMS 5.1 EDF scheduler helpers (each verified once) ────────────────────────
run_one "51_h_edf_get_context"          /opt/scripts/5.1/verify-edf-block.sh              "_Scheduler_EDF_Get_context"
run_one "51_h_edf_node_downcast"        /opt/scripts/5.1/verify-edf-block.sh              "_Scheduler_EDF_Node_downcast"
run_one "51_h_edf_map_priority"         /opt/scripts/5.1/verify-edf-map-unmap.sh          "_Scheduler_EDF_Map_priority"
run_one "51_h_edf_unmap_priority"       /opt/scripts/5.1/verify-edf-map-unmap.sh          "_Scheduler_EDF_Unmap_priority"
run_one "51_h_scheduler_get_context"    /opt/scripts/5.1/verify-edf-initialize.sh         "_Scheduler_Get_context"
run_one "51_h_rbtree_init_empty"        /opt/scripts/5.1/verify-edf-initialize.sh         "_RBTree_Initialize_empty"
run_one "51_h_node_do_initialize"       /opt/scripts/5.1/verify-edf-node-initialize.sh    "_Scheduler_Node_do_initialize"
run_one "51_h_rbtree_init_node"         /opt/scripts/5.1/verify-edf-node-initialize.sh    "_RBTree_Initialize_node"
run_one "51_h_edf_schedule_body"        /opt/scripts/5.1/verify-edf-schedule.sh           "_Scheduler_EDF_Schedule_body"
run_one "51_h_edf_extract_body"         /opt/scripts/5.1/verify-edf-schedule.sh           "_Scheduler_EDF_Extract_body"
run_one "51_h_thread_is_ready"          /opt/scripts/5.1/verify-edf-update-priority.sh    "_Thread_Is_ready"
run_one "51_h_scheduler_node_get_prio"  /opt/scripts/5.1/verify-edf-update-priority.sh    "_Scheduler_Node_get_priority"
run_one "51_h_thread_get_priority"      /opt/scripts/5.1/verify-edf-unblock.sh            "_Thread_Get_priority"
run_one "51_h_thread_sched_home_node"   /opt/scripts/5.1/verify-edf-unblock.sh            "_Thread_Scheduler_get_home_node"
run_one "51_h_priority_get_priority"    /opt/scripts/5.1/verify-edf-unblock.sh            "_Priority_Get_priority"
run_51_heir_helper "51_h_thread_get_cpu" "_Thread_Get_CPU"

# ── RTEMS 5.1 priority helpers (verify-thread-change-priority.sh) ───────────────
run_one "51_h_priority_actions_add"          /opt/scripts/5.1/verify-thread-change-priority.sh "_Priority_Actions_add"
run_one "51_h_priority_non_empty_insert"     /opt/scripts/5.1/verify-thread-change-priority.sh "_Priority_Non_empty_insert"
run_one "51_h_priority_extract_non_empty"    /opt/scripts/5.1/verify-thread-change-priority.sh "_Priority_Extract_non_empty"
run_one "51_h_priority_changed"              /opt/scripts/5.1/verify-thread-change-priority.sh "_Priority_Changed"
run_one "51_h_thread_set_sched_node_prio"    /opt/scripts/5.1/verify-thread-change-priority.sh "_Thread_Set_scheduler_node_priority"
run_one "51_h_thread_priority_action_change" /opt/scripts/5.1/verify-thread-change-priority.sh "_Thread_Priority_action_change"
run_one "51_h_thread_queue_do_nothing_pa"    /opt/scripts/5.1/verify-thread-change-priority.sh "_Thread_queue_Do_nothing_priority_actions"
run_one "51_h_thread_priority_do_perform"    /opt/scripts/5.1/verify-thread-change-priority.sh "_Thread_Priority_do_perform_actions"
run_one "51_h_thread_priority_apply"         /opt/scripts/5.1/verify-thread-change-priority.sh "_Thread_Priority_apply"
run_one "51_h_scheduler_node_set_priority"   /opt/scripts/5.1/verify-thread-change-priority.sh "_Scheduler_Node_set_priority"

# ── FreeRTOS reference top-level entry points ──────────────────────────────────
# `reference/delay.c` was reorganised: the fixed body is now just
# `xTaskDelayUntil`; the broken in-tree body lives next to it as
# `xTaskDelayUntilUnfixed` (kept as a negative reference). `vTaskDelay`
# is no longer in the reference slice. Helpers in delay.c stayed the same.
run_freertos_one "vTaskSwitchContext"       "reference/taskswitchcontext.c" "vTaskSwitchContext"
run_freertos_one "vTaskSuspend"             "reference/suspend.c"           "vTaskSuspend"
run_freertos_one "vTaskResume"              "reference/resume.c"            "vTaskResume"
run_freertos_one "xTaskDelayUntil"          "reference/delay.c"             "xTaskDelayUntil"
run_freertos_one "xTaskDelayUntilUnfixed"   "reference/delay.c"             "xTaskDelayUntilUnfixed"
run_freertos_one "xTaskIncrementTick"       "reference/incrementtick.c"     "xTaskIncrementTick"

# ── FreeRTOS task helpers ──────────────────────────────────────────────────────
run_freertos_one "h_vPortYield"                     "reference/suspend.c" "vPortYield"
run_freertos_one "h_prvTaskIsTaskSuspended"         "reference/resume.c"  "prvTaskIsTaskSuspended"
run_freertos_one "h_prvAddCurrentTaskToDelayedList" "reference/delay.c"   "prvAddCurrentTaskToDelayedList"
run_freertos_one "h_xTaskResumeAll"                 "reference/delay.c"   "xTaskResumeAll"

BENCH_T1=$(date +%s)
printf '[bench] done — %d/%d targets, total %dm%02ds\n' \
    "${CURRENT_TARGET}" "${TOTAL_TARGETS}" \
    "$(( (BENCH_T1 - BENCH_T0) / 60 ))" "$(( (BENCH_T1 - BENCH_T0) % 60 ))" >&2
