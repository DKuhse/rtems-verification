#!/bin/bash
#
# Sanity check for the FreeRTOS WP proofs.
#
# Each annotated function has an `//@ assert \false;` probe guarded by
# `#ifdef SANITY_PROBE`. The probe sits at a program point where the
# substantive postconditions live; a sound, non-vacuous proof CANNOT
# discharge it. So:
#
#   - Probe NOT proved (Timeout / Unknown / Failed)  →  PASS (model is
#       consistent at that point — the surrounding proof is meaningful).
#   - Probe proved (Valid / Qed)                     →  FAIL (hypothesis
#       set is contradictory — the surrounding proof is vacuous).
#
# This is the inverse of verify-wp-all.sh's success condition: here we
# are looking for the probe to be unprovable.
#
# Usage:
#   sanity-check.sh                       # default 10s probe timeout
#   sanity-check.sh -wp-timeout 30        # extra-paranoid 30s timeout
#
set -e
eval $(opam env)

FREERTOS_SRC="${FREERTOS_SRC:-/workspace/source/freertos-edf-msp430}"
OVERLAY="${OVERLAY:-/workspace/verification/freertos}"

MACHDEP="gcc_x86_16"

CPP_CMD="gcc -C -E \
    -D__LARGE_DATA_MODEL__ \
    -D__FRAMAC__ \
    -DEDF_SCHEDULER=1 \
    -DSANITY_PROBE=1 \
    -I${OVERLAY}/overlay/include \
    -I${OVERLAY}/stubs \
    -I${FREERTOS_SRC}/include \
    -nostdinc \
    -isystem /usr/include \
    -isystem /usr/include/x86_64-linux-gnu \
    -isystem /usr/lib/gcc/x86_64-linux-gnu/11/include"

COMMON="-machdep ${MACHDEP} -cpp-frama-c-compliant -c11"

# Short default timeout: a contradictory hypothesis set lets Qed
# discharge \false instantly. If Alt-Ergo can't prove it in 10s, we
# treat the model as consistent at that point.
DEFAULT_TIMEOUT=10
EXTRA_ARGS="$@"
if ! echo "${EXTRA_ARGS}" | grep -q -- '-wp-timeout'; then
    EXTRA_ARGS="-wp-timeout ${DEFAULT_TIMEOUT} ${EXTRA_ARGS}"
fi

PASS=0
FAIL=0

# Run WP on a single (file, function) target and inspect the probe goal.
# Args: label, source path, function name
check_probe() {
    local label="$1"
    local source_path="$2"
    local fct="$3"

    [ -f "${source_path}" ] || {
        echo "  ERROR: missing source ${source_path}"
        FAIL=$((FAIL + 1))
        return
    }

    echo "--- ${label} ---"

    # Allow `-wp-prop` etc. through ${EXTRA_ARGS}. We deliberately do
    # NOT add -wp-prop=@assert here — restricting goals would hide
    # real failures elsewhere; we want to see them too.
    output=$(frama-c \
        -cpp-command "${CPP_CMD}" \
        ${COMMON} \
        -wp -wp-fct "${fct}" -wp-model "Typed+Cast" \
        ${EXTRA_ARGS} \
        "${source_path}" 2>&1) || true

    # Look for the probe goal line. Frama-C names it like:
    #   typed_cast_<fct>_assert : Valid|Timeout|Unknown|...
    probe_line=$(echo "${output}" | grep -E "Goal typed.*${fct}_assert" | head -1 || true)

    if [ -z "${probe_line}" ]; then
        echo "  ERROR: probe goal for ${fct} not found in WP output"
        echo "         (was -DSANITY_PROBE=1 picked up? did the function name match?)"
        echo "${output}" | tail -20
        FAIL=$((FAIL + 1))
        return
    fi

    echo "  ${probe_line}"

    # Sanity check passes when the probe is NOT proved.
    if echo "${probe_line}" | grep -qE 'Valid|Qed[^:]*$'; then
        echo "  FAIL: probe proved \\false — hypothesis set is contradictory"
        FAIL=$((FAIL + 1))
    elif echo "${probe_line}" | grep -qE 'Timeout|Unknown|Failed|Stronger'; then
        echo "  PASS: probe did not prove (model consistent at probe point)"
        PASS=$((PASS + 1))
    else
        echo "  ERROR: unexpected probe status — please inspect manually"
        FAIL=$((FAIL + 1))
    fi
}

echo "========================================"
echo " Sanity Check (FreeRTOS WP probes)"
echo "========================================"
echo ""
echo "Inverted success: probes must NOT prove. A proved \\false means"
echo "the surrounding proof is vacuous. Default timeout: ${DEFAULT_TIMEOUT}s."
echo ""

check_probe "vTaskSwitchContext (overlay/tasks.c)" \
    "${OVERLAY}/overlay/tasks.c" \
    "vTaskSwitchContext"

check_probe "vTaskSwitchContext (reference/taskswitchcontext.c)" \
    "${OVERLAY}/reference/taskswitchcontext.c" \
    "vTaskSwitchContext"

check_probe "prvProcessUnblockedTask (reference/incrementtick.c)" \
    "${OVERLAY}/reference/incrementtick.c" \
    "prvProcessUnblockedTask"

check_probe "xTaskIncrementTick (reference/incrementtick.c)" \
    "${OVERLAY}/reference/incrementtick.c" \
    "xTaskIncrementTick"

echo ""
echo "========================================"
echo " Summary: ${PASS} probe(s) consistent, ${FAIL} failed"
echo "========================================"
exit ${FAIL}
