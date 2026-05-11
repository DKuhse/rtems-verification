#!/bin/bash
#
# Headless WP verification of all annotated functions on RTEMS 6.2.
#
# Runs each function group with the correct -inline-calls configuration
# as documented in CHANGES.md. Outputs per-function proof summaries.
#
# Usage:
#   verify-wp-all.sh                                # default flags
#   verify-wp-all.sh -wp-model "Typed+Cast"         # recommended memory model
#   verify-wp-all.sh -wp-timeout 30                 # 30s prover timeout
#
# Recommended invocation (matches the published 4,804/4,804 result):
#   verify-wp-all.sh -wp-model "Typed+Cast" -wp-timeout 30
#
set -e
eval $(opam env)

RTEMS_SRC="${RTEMS_SRC:-/workspace/rtems/src/rtems-6.2}"
RTEMS_PREFIX="${RTEMS_PREFIX:-/opt/rtems5}"
OVERLAY="${OVERLAY:-/workspace/verification/6.2}"

CPP_CMD="${RTEMS_PREFIX}/bin/x86_64-rtems5-gcc -C -E \
    -I${OVERLAY}/overlay/cpukit/include \
    -I${OVERLAY}/stubs \
    -I${RTEMS_SRC}/cpukit/include \
    -I${RTEMS_SRC}/cpukit/score/cpu/x86_64/include \
    -I/workspace/rtems/build/amd64/x86_64-rtems5/c/amd64/include \
    -I${RTEMS_PREFIX}/x86_64-rtems5/include \
    -I${RTEMS_PREFIX}/lib/gcc/x86_64-rtems5/9.3.0/include \
    -I${RTEMS_SRC}/bsps/include \
    -I${RTEMS_SRC}/bsps/x86_64/include \
    -I${RTEMS_SRC}/bsps/x86_64/amd64/include \
    -nostdinc"

COMMON="-machdep gcc_x86_64 -cpp-frama-c-compliant -c11"

PASS=0
FAIL=0

# Resolve stub basename to overlay path, source relpath to overlay path.
# Keeps call sites short (basename + cpukit-relative path) while
# guaranteeing absolute resolution.
run_wp() {
    local label="$1"
    local stub_basename="$2"
    local source_relpath="$3"
    local inlines="$4"
    local fcts="$5"
    shift 5
    # remaining args are extra flags (e.g. -wp-split, -wp-timeout)

    local stub="${OVERLAY}/stubs/${stub_basename}"
    local source="${OVERLAY}/overlay/cpukit/${source_relpath}"

    [ -f "${stub}" ]   || { echo "  ERROR: missing stub ${stub}";   FAIL=$((FAIL + 1)); return; }
    [ -f "${source}" ] || { echo "  ERROR: missing source ${source}"; FAIL=$((FAIL + 1)); return; }

    echo "--- ${label} ---"

    INLINE_FLAG=""
    if [ -n "${inlines}" ]; then
        INLINE_FLAG="-inline-calls ${inlines}"
    fi

    output=$(frama-c \
        -cpp-command "${CPP_CMD} -include ${stub}" \
        ${COMMON} \
        ${INLINE_FLAG} \
        -wp -wp-fct "${fcts}" \
        "$@" \
        "${source}" 2>&1)

    summary=$(echo "${output}" | grep '^\[wp\] Proved goals:' || true)
    if [ -z "${summary}" ]; then
        echo "  ERROR: no proof summary found"
        echo "${output}" | tail -10
        FAIL=$((FAIL + 1))
        return
    fi

    proved=$(echo "${summary}" | sed 's/.*: *\([0-9]*\) *\/ *\([0-9]*\)/\1/')
    total=$(echo "${summary}" | sed 's/.*: *\([0-9]*\) *\/ *\([0-9]*\)/\2/')
    echo "  ${summary}"

    if [ "${proved}" = "${total}" ]; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        echo "${output}" | grep -E "Timeout|Unknown|Failed" | head -10
    fi
}

EXTRA_ARGS="$@"

echo "========================================"
echo " Headless WP Verification (RTEMS 6.2)"
echo "========================================"
echo ""

# ── Thread Priority ──────────────────────────────────────────────
# All 7 thread priority functions need -wp-split (without it, WP
# generates 3 coarse goals per ensures instead of 12 fine-grained ones).
BASE_INLINES="_Priority_Node_is_active,_Priority_Extract_non_empty,_Priority_Non_empty_insert,_Priority_Changed"

echo "== Thread Priority (score/src/threadchangepriority.c) =="
echo ""

# Group 1: base functions
run_wp "_Thread_Set_scheduler_node_priority" \
    stubs.h "score/src/threadchangepriority.c" \
    "${BASE_INLINES}" \
    "_Thread_Set_scheduler_node_priority" \
    -wp-split ${EXTRA_ARGS}

run_wp "_Thread_Priority_action_change" \
    stubs.h "score/src/threadchangepriority.c" \
    "${BASE_INLINES}" \
    "_Thread_Priority_action_change" \
    -wp-split ${EXTRA_ARGS}

run_wp "_Thread_Priority_do_perform_actions" \
    stubs.h "score/src/threadchangepriority.c" \
    "${BASE_INLINES}" \
    "_Thread_Priority_do_perform_actions" \
    -wp-split ${EXTRA_ARGS}

# Group 2: _Thread_Priority_apply needs _do_perform_actions inlined
run_wp "_Thread_Priority_apply" \
    stubs.h "score/src/threadchangepriority.c" \
    "${BASE_INLINES},_Thread_Priority_do_perform_actions" \
    "_Thread_Priority_apply" \
    -wp-split ${EXTRA_ARGS}

# Group 3: _add, _remove, _changed need both _apply and _do_perform_actions inlined
FULL_INLINES="${BASE_INLINES},_Thread_Priority_do_perform_actions,_Thread_Priority_apply"

run_wp "_Thread_Priority_add" \
    stubs.h "score/src/threadchangepriority.c" \
    "${FULL_INLINES}" \
    "_Thread_Priority_add" \
    -wp-split ${EXTRA_ARGS}

run_wp "_Thread_Priority_remove" \
    stubs.h "score/src/threadchangepriority.c" \
    "${FULL_INLINES}" \
    "_Thread_Priority_remove" \
    -wp-split ${EXTRA_ARGS}

run_wp "_Thread_Priority_changed" \
    stubs.h "score/src/threadchangepriority.c" \
    "${FULL_INLINES}" \
    "_Thread_Priority_changed" \
    -wp-split ${EXTRA_ARGS}

echo ""

# ── EDF Update Priority ─────────────────────────────────────────
# In 6.2, _Scheduler_Update_heir/_Scheduler_EDF_Schedule_body have been
# replaced by the _Scheduler_uniprocessor_* abstraction layer.
echo "== EDF Update Priority (score/src/scheduleredfchangepriority.c) =="
echo ""

run_wp "_Scheduler_EDF_Update_priority" \
    stubs.h "score/src/scheduleredfchangepriority.c" \
    "_Scheduler_uniprocessor_Schedule,_Scheduler_EDF_Get_highest_ready,_Scheduler_uniprocessor_Update_heir_if_preemptible,_Scheduler_uniprocessor_Update_heir" \
    "_Scheduler_EDF_Update_priority" \
    ${EXTRA_ARGS}

echo ""

# ── EDF Unblock ──────────────────────────────────────────────────
echo "== EDF Unblock (score/src/scheduleredfunblock.c) =="
echo ""

run_wp "_Scheduler_EDF_Unblock" \
    stubs.h "score/src/scheduleredfunblock.c" \
    "_Scheduler_uniprocessor_Unblock,_Scheduler_uniprocessor_Update_heir_if_preemptible,_Scheduler_uniprocessor_Update_heir" \
    "_Scheduler_EDF_Unblock" \
    ${EXTRA_ARGS}

echo ""

# ── EDF Release and Cancel ───────────────────────────────────────
echo "== EDF Release/Cancel (score/src/scheduleredfreleasejob.c) =="
echo ""

run_wp "_Scheduler_EDF_Map_priority" \
    release_cancel_stubs.h "score/src/scheduleredfreleasejob.c" \
    "" \
    "_Scheduler_EDF_Map_priority" \
    ${EXTRA_ARGS}

run_wp "_Scheduler_EDF_Unmap_priority" \
    release_cancel_stubs.h "score/src/scheduleredfreleasejob.c" \
    "" \
    "_Scheduler_EDF_Unmap_priority" \
    ${EXTRA_ARGS}

run_wp "_Scheduler_EDF_Release_job" \
    release_cancel_stubs.h "score/src/scheduleredfreleasejob.c" \
    "" \
    "_Scheduler_EDF_Release_job" \
    ${EXTRA_ARGS}

run_wp "_Scheduler_EDF_Cancel_job" \
    release_cancel_stubs.h "score/src/scheduleredfreleasejob.c" \
    "" \
    "_Scheduler_EDF_Cancel_job" \
    ${EXTRA_ARGS}

echo ""
echo "========================================"
echo " Summary: ${PASS} passed, ${FAIL} with unproved goals"
echo "========================================"
exit ${FAIL}
