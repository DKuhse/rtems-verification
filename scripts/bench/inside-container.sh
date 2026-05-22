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
# line per target on stdout. Per-target Frama-C logs land in /tmp/wp-logs/.
#
# Env vars:
#   WP_PAR        when set, appends "-wp-par $WP_PAR" to every invocation
#   LOG_DIR       override /tmp/wp-logs
#
# Drives are split by how the underlying verify-*.sh script handles function
# selection:
#   - run_one(...)             — for scripts that honour WP_FCTS
#   - run_uni_helper(...)      — direct frama-c against the scheduleruni
#                                harness (whose script hard-codes -wp-fct)
#   - run_freertos_one(...)    — direct frama-c against the FreeRTOS
#                                reference slices (which hard-code -wp-fct)
set -u

LOG_DIR="${LOG_DIR:-/tmp/wp-logs}"
mkdir -p "${LOG_DIR}"

EXTRA_ARGS=()
if [ -n "${WP_PAR:-}" ]; then
    EXTRA_ARGS+=(-wp-par "${WP_PAR}")
fi

if command -v opam >/dev/null 2>&1; then
    eval "$(opam env)"
fi

RTEMS_SRC="${RTEMS_SRC:-/workspace/rtems/src/rtems-6.2-pristine}"
RTEMS_PREFIX="${RTEMS_PREFIX:-/opt/rtems5}"
OVERLAY_62="${OVERLAY_62:-/workspace/verification/6.2}"
RTEMS_BUILD_BSP="${RTEMS_BUILD_BSP:-/workspace/rtems/build/amd64/x86_64-rtems5/c/amd64/include}"

# Parses /tmp/wp-logs/${label}.log and emits the RESULT line.
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

# Direct invocation against the scheduleruni-unblock harness (verify-script
# hard-codes -wp-fct, so we can't ride WP_FCTS).
run_uni_helper() {
    local label="$1" fct="$2"
    local log="${LOG_DIR}/${label}.log"
    local SRC="${OVERLAY_62}/harnesses/scheduleruni-unblock-harness.c"

    local CPP_CMD="${RTEMS_PREFIX}/bin/x86_64-rtems5-gcc -C -E \
        -D__FRAMAC__ -D__rtems__ \
        -I${OVERLAY_62}/overlay/cpukit/include \
        -I${OVERLAY_62}/models \
        -I${RTEMS_SRC}/cpukit/include \
        -I${RTEMS_SRC}/cpukit/score/cpu/x86_64/include \
        -I${RTEMS_BUILD_BSP} \
        -I${RTEMS_PREFIX}/x86_64-rtems5/include \
        -I${RTEMS_PREFIX}/lib/gcc/x86_64-rtems5/9.3.0/include \
        -I${RTEMS_SRC}/bsps/include \
        -I${RTEMS_SRC}/bsps/x86_64/include \
        -I${RTEMS_SRC}/bsps/x86_64/amd64/include \
        -nostdinc"

    local t0 t1
    t0=$(date +%s.%N)
    frama-c \
        -cpp-command "${CPP_CMD}" \
        -machdep gcc_x86_64 -cpp-frama-c-compliant -std c11 \
        "${SRC}" \
        -volatile -then-on Volatile \
        -wp -wp-fct "${fct}" -wp-model "Typed+Cast" -wp-timeout 30 \
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
        -wp -wp-fct "${fct}" -wp-model "Typed+Cast" -wp-timeout 30 \
        -wp-cache none \
        "${EXTRA_ARGS[@]}" \
        >"${log}" 2>&1
    local rc=$?
    t1=$(date +%s.%N)
    emit_result "${label}" "$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.2f", b-a}')" "${rc}"
}

# ── RTEMS 6.2 top-level entry points ────────────────────────────────────────────
run_one "edf_initialize"          /opt/scripts/6.2/verify-edf-initialize.sh         "_Scheduler_EDF_Initialize"
run_one "edf_node_initialize"     /opt/scripts/6.2/verify-edf-node-initialize.sh    "_Scheduler_EDF_Node_initialize"
run_one "edf_block"               /opt/scripts/6.2/verify-edf-block.sh              "_Scheduler_EDF_Block"
run_one "edf_schedule"            /opt/scripts/6.2/verify-edf-schedule.sh           "_Scheduler_EDF_Schedule"
run_one "edf_yield"               /opt/scripts/6.2/verify-edf-yield.sh              "_Scheduler_EDF_Yield"
run_one "edf_unblock"             /opt/scripts/6.2/verify-edf-unblock.sh            "_Scheduler_EDF_Unblock"
run_one "edf_update_priority"     /opt/scripts/6.2/verify-edf-update-priority.sh    "_Scheduler_EDF_Update_priority"
run_one "edf_release_job"         /opt/scripts/6.2/verify-edf-release-cancel.sh     "_Scheduler_EDF_Release_job"
run_one "edf_cancel_job"          /opt/scripts/6.2/verify-edf-release-cancel.sh     "_Scheduler_EDF_Cancel_job"
# Uniprocessor Unblock — verify-scheduleruni-unblock.sh hard-codes 4 fcts,
# so we drive it directly to isolate just _Scheduler_uniprocessor_Unblock.
run_uni_helper  "scheduleruni_unblock"    "_Scheduler_uniprocessor_Unblock"
run_one "scheduler_release_job"   /opt/scripts/6.2/verify-scheduler-release-job.sh  "_Scheduler_Release_job"
run_one "thread_priority_add"     /opt/scripts/6.2/verify-thread-change-priority.sh "_Thread_Priority_add"
run_one "thread_priority_changed" /opt/scripts/6.2/verify-thread-change-priority.sh "_Thread_Priority_changed"
run_one "thread_priority_remove"  /opt/scripts/6.2/verify-thread-change-priority.sh "_Thread_Priority_remove"
run_one "ratemon_release_job"     /opt/scripts/6.2/verify-ratemon-release-job.sh    "_Rate_monotonic_Release_job"
run_one "ratemon_cancel"          /opt/scripts/6.2/verify-ratemon-cancel.sh         "_Rate_monotonic_Cancel"

# ── RTEMS 6.2 EDF scheduler helpers (each verified once) ────────────────────────
run_one        "h_edf_get_context"            /opt/scripts/6.2/verify-edf-block.sh             "_Scheduler_EDF_Get_context"
run_one        "h_edf_node_downcast"          /opt/scripts/6.2/verify-edf-block.sh             "_Scheduler_EDF_Node_downcast"
run_one        "h_edf_map_priority"           /opt/scripts/6.2/verify-edf-release-cancel.sh    "_Scheduler_EDF_Map_priority"
run_one        "h_edf_unmap_priority"         /opt/scripts/6.2/verify-edf-release-cancel.sh    "_Scheduler_EDF_Unmap_priority"
run_one        "h_scheduler_get_context"      /opt/scripts/6.2/verify-edf-initialize.sh        "_Scheduler_Get_context"
run_one        "h_rbtree_init_empty"          /opt/scripts/6.2/verify-edf-initialize.sh        "_RBTree_Initialize_empty"
run_one        "h_node_do_initialize"         /opt/scripts/6.2/verify-edf-node-initialize.sh   "_Scheduler_Node_do_initialize"
run_one        "h_rbtree_init_node"           /opt/scripts/6.2/verify-edf-node-initialize.sh   "_RBTree_Initialize_node"
run_one        "h_thread_is_ready"            /opt/scripts/6.2/verify-edf-schedule.sh          "_Thread_Is_ready"
run_uni_helper "h_uni_update_heir"                "_Scheduler_uniprocessor_Update_heir"
run_uni_helper "h_uni_update_heir_if_necessary"   "_Scheduler_uniprocessor_Update_heir_if_necessary"
run_uni_helper "h_uni_update_heir_if_preemptible" "_Scheduler_uniprocessor_Update_heir_if_preemptible"

# ── RTEMS 6.2 priority helpers ─────────────────────────────────────────────────
run_one "h_priority_actions_add"          /opt/scripts/6.2/verify-thread-change-priority.sh "_Priority_Actions_add"
run_one "h_priority_non_empty_insert"     /opt/scripts/6.2/verify-thread-change-priority.sh "_Priority_Non_empty_insert"
run_one "h_priority_extract_non_empty"    /opt/scripts/6.2/verify-thread-change-priority.sh "_Priority_Extract_non_empty"
run_one "h_priority_changed"              /opt/scripts/6.2/verify-thread-change-priority.sh "_Priority_Changed"
run_one "h_thread_set_sched_node_prio"    /opt/scripts/6.2/verify-thread-change-priority.sh "_Thread_Set_scheduler_node_priority"
run_one "h_thread_priority_action_change" /opt/scripts/6.2/verify-thread-change-priority.sh "_Thread_Priority_action_change"

# ── FreeRTOS reference top-level entry points ──────────────────────────────────
run_freertos_one "vTaskSwitchContext" "reference/taskswitchcontext.c" "vTaskSwitchContext"
run_freertos_one "vTaskSuspend"       "reference/suspend.c"           "vTaskSuspend"
run_freertos_one "vTaskResume"        "reference/resume.c"            "vTaskResume"
run_freertos_one "vTaskDelay"         "reference/delay.c"             "vTaskDelay"
run_freertos_one "xTaskDelayUntil"    "reference/delay.c"             "xTaskDelayUntil"
run_freertos_one "xTaskIncrementTick" "reference/incrementtick.c"     "xTaskIncrementTick"

# ── FreeRTOS task helpers ──────────────────────────────────────────────────────
run_freertos_one "h_vPortYield"                     "reference/suspend.c" "vPortYield"
run_freertos_one "h_prvTaskIsTaskSuspended"         "reference/resume.c"  "prvTaskIsTaskSuspended"
run_freertos_one "h_xTaskDelayUntilReadyRefresh"    "reference/delay.c"   "xTaskDelayUntilReadyRefresh"
run_freertos_one "h_prvAddCurrentTaskToDelayedList" "reference/delay.c"   "prvAddCurrentTaskToDelayedList"
run_freertos_one "h_xTaskResumeAll"                 "reference/delay.c"   "xTaskResumeAll"
