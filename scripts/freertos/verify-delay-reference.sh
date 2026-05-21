#!/bin/bash
#
# Headless WP run for the standalone reference extraction of
# vTaskDelay and xTaskDelayUntil (verification/freertos/reference/delay.c).
#
# Usage:
#   verify-delay-reference.sh                       # default flags
#   verify-delay-reference.sh -wp-timeout 60        # 60s prover timeout
#   RUN_SOUNDNESS_PROBE=0 verify-delay-reference.sh # skip negative probe
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
    -I${OVERLAY}/model \
    -I${OVERLAY}/overlay/include \
    -I${OVERLAY}/stubs \
    -I${FREERTOS_SRC}/include \
    -nostdinc \
    -isystem /usr/include \
    -isystem /usr/include/x86_64-linux-gnu \
    -isystem /usr/lib/gcc/x86_64-linux-gnu/11/include"

DELAY_SOURCE="${OVERLAY}/reference/delay.c"

run_soundness_probe() {
    local function_name="$1"
    shift

    local probe_source
    local report
    local output
    local probe_lines

    probe_source=$(mktemp --suffix=.c)
    report=$(mktemp --suffix=.json)

    awk '
        {
            print
            if ($0 ~ /traceENTER_xTaskDelayUntil\(pxPreviousWakeTime, xTimeIncrement\);/) {
                print "    //@ assert \\false;"
                inserted = 1
            }
        }
        END {
            if (!inserted) {
                exit 2
            }
        }
    ' "${DELAY_SOURCE}" > "${probe_source}" || {
        rm -f "${probe_source}" "${report}"
        echo "ERROR: could not inject ${function_name} soundness probe"
        exit 1
    }

    output=$(frama-c \
        -cpp-command "${CPP_CMD}" \
        -machdep "${MACHDEP}" -cpp-frama-c-compliant -std c11 \
        -wp -wp-fct "${function_name}" -wp-model "Typed+Cast" \
        -wp-report-json "${report}" \
        -wp-timeout 10 \
        "$@" \
        "${probe_source}" 2>&1) || true

    probe_lines=$(awk -v function_name="${function_name}" '
        /"goal":/ {
            goal = $0
            is_probe = (index($0, function_name "_assert") > 0)
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
        echo "ERROR: ${function_name} soundness probe goal was not found"
        echo "${output}" | tail -20
        exit 1
    fi

    echo "--- ${function_name} soundness probe ---"
    echo "${probe_lines}" | sed 's/^/  /'

    if echo "${probe_lines}" | grep -vqE ' : (valid|timeout|unknown|failed)$'; then
        echo "ERROR: unexpected ${function_name} probe verdict"
        exit 1
    fi

    if ! echo "${probe_lines}" | grep -qE ' : (timeout|unknown|failed)$'; then
        echo "FAIL: all ${function_name} probe goals proved \\false; its hypotheses are contradictory"
        exit 1
    fi

    echo "PASS: at least one ${function_name} probe goal did not prove \\false"
    echo ""
}

echo "========================================"
echo " WP Verification (FreeRTOS reference)"
echo "========================================"
echo ""

if [ "${RUN_SOUNDNESS_PROBE:-1}" != "0" ]; then
    run_soundness_probe xTaskDelayUntil "$@"
    run_soundness_probe xTaskDelayUntilReadyRefresh "$@"
fi

echo "--- vTaskDelay / xTaskDelayUntil (reference) ---"

frama-c \
    -cpp-command "${CPP_CMD}" \
    -machdep "${MACHDEP}" -cpp-frama-c-compliant -std c11 \
    -wp -wp-fct vTaskDelay,xTaskDelayUntil,xTaskDelayUntilReadyRefresh,prvAddCurrentTaskToDelayedList,xTaskResumeAll,vPortYield -wp-model "Typed+Cast" \
    -wp-timeout 10 \
    "$@" \
    "${DELAY_SOURCE}"
