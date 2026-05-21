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
    -I${OVERLAY}/model \
    -I${OVERLAY}/overlay/include \
    -I${OVERLAY}/stubs \
    -I${FREERTOS_SRC}/include \
    -nostdinc \
    -isystem /usr/include \
    -isystem /usr/include/x86_64-linux-gnu \
    -isystem /usr/lib/gcc/x86_64-linux-gnu/11/include"

CPP_CMD_NORMAL="gcc -C -E \
    -D__LARGE_DATA_MODEL__ \
    -D__FRAMAC__ \
    -DEDF_SCHEDULER=1 \
    -I${OVERLAY}/model \
    -I${OVERLAY}/overlay/include \
    -I${OVERLAY}/stubs \
    -I${FREERTOS_SRC}/include \
    -nostdinc \
    -isystem /usr/include \
    -isystem /usr/include/x86_64-linux-gnu \
    -isystem /usr/lib/gcc/x86_64-linux-gnu/11/include"

COMMON="-machdep ${MACHDEP} -cpp-frama-c-compliant -std c11"

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
    local report

    [ -f "${source_path}" ] || {
        echo "  ERROR: missing source ${source_path}"
        FAIL=$((FAIL + 1))
        return
    }

    echo "--- ${label} ---"

    # Allow `-wp-prop` etc. through ${EXTRA_ARGS}. We deliberately do
    # NOT add -wp-prop=@assert here — restricting goals would hide
    # real failures elsewhere; we want to see them too.
    report=$(mktemp --suffix=.json)
    output=$(frama-c \
        -cpp-command "${CPP_CMD}" \
        ${COMMON} \
        -wp -wp-fct "${fct}" -wp-model "Typed+Cast" \
        -wp-report-json "${report}" \
        ${EXTRA_ARGS} \
        "${source_path}" 2>&1) || true

    # Read the machine-readable WP report. Frama-C 32 only prints failed
    # goals in the normal text output, so a proved probe may have no
    # human-readable goal line at all.
    probe_lines=$(awk -v fct="${fct}" '
        /"goal":/ {
            goal = $0
            is_probe = ($0 ~ /(sanity_|vacuity_)/)
        }
        is_probe && /"verdict":/ {
            verdict = $0
            sub(/^.*"goal": "/, "", goal)
            sub(/".*$/, "", goal)
            sub(/^.*"verdict": "/, "", verdict)
            sub(/".*$/, "", verdict)
            found = 1
            print goal " : " verdict
        }
        END {
            if (!found) exit 1
        }
    ' "${report}" 2>/dev/null || true)
    rm -f "${report}"

    if [ -z "${probe_lines}" ]; then
        echo "  ERROR: probe goal for ${fct} not found in WP output"
        echo "         (was -DSANITY_PROBE=1 picked up? did the function name match?)"
        echo "${output}" | tail -20
        FAIL=$((FAIL + 1))
        return
    fi

    local function_failed=0
    local function_passed=0
    while IFS= read -r probe_line; do
        [ -n "${probe_line}" ] || continue
        echo "  ${probe_line}"

        # Sanity check passes when the probe is NOT proved.
        if echo "${probe_line}" | grep -qE ' : valid$'; then
            echo "  FAIL: probe proved \\false — hypothesis set is contradictory"
            function_failed=1
        elif echo "${probe_line}" | grep -qE ' : (timeout|unknown|failed)$'; then
            function_passed=1
        else
            echo "  ERROR: unexpected probe status — please inspect manually"
            function_failed=1
        fi
    done <<< "${probe_lines}"

    if [ "${function_failed}" -ne 0 ]; then
        FAIL=$((FAIL + 1))
    elif [ "${function_passed}" -ne 0 ]; then
        echo "  PASS: no probe proved (model consistent at checked probe points)"
        PASS=$((PASS + 1))
    else
        echo "  ERROR: no usable probe verdicts found"
        FAIL=$((FAIL + 1))
    fi
}

# Same check, but injects a temporary named `assert \false` probe at a
# specific source marker. This lets us cover functions that do not have a
# permanent SANITY_PROBE block without changing the verification source.
# Args: label, source path, function name, marker text, before|after
check_injected_probe() {
    local label="$1"
    local source_path="$2"
    local fct="$3"
    local marker="$4"
    local where="${5:-before}"
    local probe_source
    local report

    [ -f "${source_path}" ] || {
        echo "  ERROR: missing source ${source_path}"
        FAIL=$((FAIL + 1))
        return
    }

    echo "--- ${label} ---"

    probe_source=$(mktemp --suffix=.c)
    awk -v marker="${marker}" -v where="${where}" '
        {
            if (!inserted && index($0, marker) > 0 && where == "before") {
                print "    //@ assert vacuity_probe: \\false;"
                inserted = 1
            }
            print
            if (!inserted && index($0, marker) > 0 && where != "before") {
                print "    //@ assert vacuity_probe: \\false;"
                inserted = 1
            }
        }
        END {
            if (!inserted) exit 2
        }
    ' "${source_path}" > "${probe_source}" || {
        rm -f "${probe_source}"
        echo "  ERROR: could not inject probe at marker: ${marker}"
        FAIL=$((FAIL + 1))
        return
    }

    report=$(mktemp --suffix=.json)
    output=$(frama-c \
        -cpp-command "${CPP_CMD_NORMAL}" \
        ${COMMON} \
        -wp -wp-fct "${fct}" -wp-model "Typed+Cast" \
        -wp-report-json "${report}" \
        ${EXTRA_ARGS} \
        "${probe_source}" 2>&1) || true

    probe_lines=$(awk '
        /"goal":/ {
            goal = $0
            is_probe = ($0 ~ /vacuity_probe/)
        }
        is_probe && /"verdict":/ {
            verdict = $0
            sub(/^.*"goal": "/, "", goal)
            sub(/".*$/, "", goal)
            sub(/^.*"verdict": "/, "", verdict)
            sub(/".*$/, "", verdict)
            found = 1
            print goal " : " verdict
        }
        END {
            if (!found) exit 1
        }
    ' "${report}" 2>/dev/null || true)
    rm -f "${probe_source}" "${report}"

    if [ -z "${probe_lines}" ]; then
        echo "  ERROR: injected probe goal for ${fct} not found in WP output"
        echo "         marker: ${marker}"
        echo "${output}" | tail -20
        FAIL=$((FAIL + 1))
        return
    fi

    local function_failed=0
    local function_passed=0
    while IFS= read -r probe_line; do
        [ -n "${probe_line}" ] || continue
        echo "  ${probe_line}"

        if echo "${probe_line}" | grep -qE ' : valid$'; then
            echo "  FAIL: probe proved \\false — hypothesis set is contradictory"
            function_failed=1
        elif echo "${probe_line}" | grep -qE ' : (timeout|unknown|failed)$'; then
            function_passed=1
        else
            echo "  ERROR: unexpected probe status — please inspect manually"
            function_failed=1
        fi
    done <<< "${probe_lines}"

    if [ "${function_failed}" -ne 0 ]; then
        FAIL=$((FAIL + 1))
    elif [ "${function_passed}" -ne 0 ]; then
        echo "  PASS: probe did not prove (model consistent at checked probe point)"
        PASS=$((PASS + 1))
    else
        echo "  ERROR: no usable probe verdicts found"
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

check_probe "xTaskIncrementTick (reference/incrementtick.c)" \
    "${OVERLAY}/reference/incrementtick.c" \
    "xTaskIncrementTick"

check_injected_probe "vTaskResume end (reference/resume.c)" \
    "${OVERLAY}/reference/resume.c" \
    "vTaskResume" \
    "traceRETURN_vTaskResume();" \
    "before"

check_injected_probe "vTaskResume entry (reference/resume.c)" \
    "${OVERLAY}/reference/resume.c" \
    "vTaskResume" \
    "traceENTER_vTaskResume(xTaskToResume);" \
    "after"

check_injected_probe "vTaskResume after suspended-list remove (reference/resume.c)" \
    "${OVERLAY}/reference/resume.c" \
    "vTaskResume" \
    "(void)uxListRemove(&(pxTCB->xStateListItem));" \
    "after"

check_injected_probe "vTaskResume after ready-list insert (reference/resume.c)" \
    "${OVERLAY}/reference/resume.c" \
    "vTaskResume" \
    "prvAddTaskToReadyList(pxTCB);" \
    "after"

check_injected_probe "vTaskResume after preemption check (reference/resume.c)" \
    "${OVERLAY}/reference/resume.c" \
    "vTaskResume" \
    "taskYIELD_ANY_CORE_IF_USING_PREEMPTION(pxTCB);" \
    "after"

check_injected_probe "prvTaskIsTaskSuspended end (reference/resume.c)" \
    "${OVERLAY}/reference/resume.c" \
    "prvTaskIsTaskSuspended" \
    "return xReturn;" \
    "before"

check_injected_probe "vPortYield end (reference/resume.c)" \
    "${OVERLAY}/reference/resume.c" \
    "vPortYield" \
    "portRESTORE_CONTEXT();" \
    "after"

check_injected_probe "vTaskSuspend end (reference/suspend.c)" \
    "${OVERLAY}/reference/suspend.c" \
    "vTaskSuspend" \
    "traceRETURN_vTaskSuspend();" \
    "before"

check_injected_probe "vPortYield end (reference/suspend.c)" \
    "${OVERLAY}/reference/suspend.c" \
    "vPortYield" \
    "portRESTORE_CONTEXT();" \
    "after"

check_injected_probe "xTaskDelayUntil end (reference/delay.c)" \
    "${OVERLAY}/reference/delay.c" \
    "xTaskDelayUntil" \
    "traceRETURN_xTaskDelayUntil(xShouldDelay);" \
    "before"

check_injected_probe "vTaskDelay end (reference/delay.c)" \
    "${OVERLAY}/reference/delay.c" \
    "vTaskDelay" \
    "traceRETURN_vTaskDelay();" \
    "before"

check_injected_probe "prvAddCurrentTaskToDelayedList end (reference/delay.c)" \
    "${OVERLAY}/reference/delay.c" \
    "prvAddCurrentTaskToDelayedList" \
    "(void)xCanBlockIndefinitely;" \
    "before"

check_injected_probe "xTaskResumeAll end (reference/delay.c)" \
    "${OVERLAY}/reference/delay.c" \
    "xTaskResumeAll" \
    "traceRETURN_xTaskResumeAll(xAlreadyYielded);" \
    "before"

check_injected_probe "vPortYield end (reference/delay.c)" \
    "${OVERLAY}/reference/delay.c" \
    "vPortYield" \
    "portRESTORE_CONTEXT();" \
    "after"

echo ""
echo "========================================"
echo " Summary: ${PASS} probe(s) consistent, ${FAIL} failed"
echo "========================================"
exit ${FAIL}
