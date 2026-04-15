#!/bin/bash
#
# Headless WP verification of all annotated functions.
#
# Runs each function group with the correct -inline-calls configuration
# as documented in the README. Outputs per-function proof summaries.
#
# Usage:
#   verify-wp-all.sh                  # default 10s prover timeout
#   verify-wp-all.sh -wp-timeout 30   # 30s timeout (passes through to frama-c)
#
set -e
eval $(opam env)

RTEMS_HOME="${RTEMS_HOME:-$HOME/RTEMS}"
RTEMS_PREFIX="${RTEMS_HOME}/rtems_x86_64"
RTEMS_SRC="${RTEMS_HOME}/src/rtems-5.1"

cd "${RTEMS_SRC}/cpukit"

CPP_CMD="${RTEMS_PREFIX}/bin/x86_64-rtems5-gcc -C -E \
    -I./include -I./score/cpu/x86_64/include/ \
    -I../../../build/amd64/x86_64-rtems5/c/amd64/include/ \
    -I${RTEMS_PREFIX}/x86_64-rtems5/include \
    -I${RTEMS_PREFIX}/lib/gcc/x86_64-rtems5/9.3.0/include \
    -nostdinc"

COMMON="-machdep gcc_x86_64 -cpp-frama-c-compliant -c11"

PASS=0
FAIL=0

run_wp() {
    local label="$1"
    local stubs="$2"
    local source="$3"
    local inlines="$4"
    local fcts="$5"
    shift 5
    # remaining args are extra flags (e.g. -wp-timeout)

    echo "--- ${label} ---"

    INLINE_FLAG=""
    if [ -n "${inlines}" ]; then
        INLINE_FLAG="-inline-calls ${inlines}"
    fi

    output=$(frama-c \
        -cpp-command "${CPP_CMD} -include ${stubs}" \
        ${COMMON} \
        ${INLINE_FLAG} \
        -wp -wp-fct "${fcts}" \
        "$@" \
        "${source}" 2>&1)

    summary=$(echo "${output}" | grep '^\[wp\] Proved goals:' || true)
    if [ -z "${summary}" ]; then
        echo "  ERROR: no proof summary found"
        echo "${output}" | tail -5
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
        # Show which goals failed
        echo "${output}" | grep -E "Timeout|Unknown|Failed" | head -10
    fi
}

EXTRA_ARGS="$@"

echo "========================================"
echo " Headless WP Verification"
echo "========================================"
echo ""

# ── Thread Priority ──────────────────────────────────────────────
# Base inline-calls from README
BASE_INLINES="_Priority_Node_is_active,_Priority_Extract_non_empty,_Priority_Non_empty_insert,_Priority_Changed"

echo "== Thread Priority (score/src/threadchangepriority.c) =="
echo ""

# Group 1: base functions (use base inline-calls)
run_wp "_Thread_Set_scheduler_node_priority" \
    stubs.h "score/src/threadchangepriority.c" \
    "${BASE_INLINES}" \
    "_Thread_Set_scheduler_node_priority" \
    ${EXTRA_ARGS}

run_wp "_Thread_Priority_action_change" \
    stubs.h "score/src/threadchangepriority.c" \
    "${BASE_INLINES}" \
    "_Thread_Priority_action_change" \
    ${EXTRA_ARGS}

run_wp "_Thread_Priority_do_perform_actions" \
    stubs.h "score/src/threadchangepriority.c" \
    "${BASE_INLINES}" \
    "_Thread_Priority_do_perform_actions" \
    ${EXTRA_ARGS}

# Group 2: _Thread_Priority_apply needs _Thread_Priority_do_perform_actions inlined too
run_wp "_Thread_Priority_apply" \
    stubs.h "score/src/threadchangepriority.c" \
    "${BASE_INLINES},_Thread_Priority_do_perform_actions" \
    "_Thread_Priority_apply" \
    ${EXTRA_ARGS}

# Group 3: _add, _remove, _changed need both _apply and _do_perform_actions inlined
FULL_INLINES="${BASE_INLINES},_Thread_Priority_do_perform_actions,_Thread_Priority_apply"

run_wp "_Thread_Priority_add" \
    stubs.h "score/src/threadchangepriority.c" \
    "${FULL_INLINES}" \
    "_Thread_Priority_add" \
    ${EXTRA_ARGS}

run_wp "_Thread_Priority_remove" \
    stubs.h "score/src/threadchangepriority.c" \
    "${FULL_INLINES}" \
    "_Thread_Priority_remove" \
    ${EXTRA_ARGS}

run_wp "_Thread_Priority_changed" \
    stubs.h "score/src/threadchangepriority.c" \
    "${FULL_INLINES}" \
    "_Thread_Priority_changed" \
    ${EXTRA_ARGS}

echo ""

# ── EDF Update Priority ─────────────────────────────────────────
echo "== EDF Update Priority (score/src/scheduleredfchangepriority.c) =="
echo ""

run_wp "_Scheduler_EDF_Update_priority" \
    stubs.h "score/src/scheduleredfchangepriority.c" \
    "_Scheduler_Update_heir,_Scheduler_EDF_Schedule_body" \
    "_Scheduler_EDF_Update_priority" \
    ${EXTRA_ARGS}

echo ""

# ── EDF Unblock ──────────────────────────────────────────────────
echo "== EDF Unblock (score/src/scheduleredfunblock.c) =="
echo ""

run_wp "_Scheduler_EDF_Unblock" \
    stubs.h "score/src/scheduleredfunblock.c" \
    "_Scheduler_Update_heir" \
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
