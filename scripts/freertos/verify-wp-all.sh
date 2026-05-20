#!/bin/bash
#
# Smoke-test WP run for the FreeRTOS EDF port.
#
# At this stage there are no annotated functions — this script just
# preprocesses list.c through Frama-C with the MSP430-flavoured toolchain
# and runs WP, so we can confirm the Docker setup is wired up correctly.
# Real verification targets get added later, one block per function group.
#
# Usage:
#   verify-wp-all.sh                            # default flags
#   verify-wp-all.sh -wp-timeout 30             # 30s prover timeout
#
set -e
eval $(opam env)

FREERTOS_SRC="${FREERTOS_SRC:-/workspace/source/freertos-edf-msp430}"
OVERLAY="${OVERLAY:-/workspace/verification/freertos}"

# Frama-C has no native MSP430 machdep. gcc_x86_16 is the closest stock
# match: 16-bit int/short, 32-bit long, signed char — same scalar layout as
# MSP430 with the large data model (__MSP430X_LARGE__ / __LARGE_DATA_MODEL__),
# which is what FreeRTOSConfig.h's heap-size branch assumes.
MACHDEP="gcc_x86_16"

CPP_CMD="gcc -C -E \
    -D__LARGE_DATA_MODEL__ \
    -D__FRAMAC__ \
    -DEDF_SCHEDULER=1 \
    -I${OVERLAY}/overlay/include \
    -I${OVERLAY}/stubs \
    -I${FREERTOS_SRC}/include \
    -nostdinc \
    -isystem /usr/include \
    -isystem /usr/include/x86_64-linux-gnu \
    -isystem /usr/lib/gcc/x86_64-linux-gnu/11/include"

COMMON="-machdep ${MACHDEP} -cpp-frama-c-compliant -std c11"

PASS=0
FAIL=0

run_wp() {
    local label="$1"
    local source_path="$2"
    local fcts="$3"
    shift 3

    [ -f "${source_path}" ] || { echo "  ERROR: missing source ${source_path}"; FAIL=$((FAIL + 1)); return; }

    echo "--- ${label} ---"

    local fct_flag=""
    if [ -n "${fcts}" ]; then
        fct_flag="-wp-fct ${fcts}"
    fi

    output=$(frama-c \
        -cpp-command "${CPP_CMD}" \
        ${COMMON} \
        -wp ${fct_flag} \
        "$@" \
        "${source_path}" 2>&1)

    summary=$(echo "${output}" | grep '^\[wp\] Proved goals:' || true)
    if [ -z "${summary}" ]; then
        # No proof obligations is expected for unannotated code — treat as pass
        # but flag any hard errors in the output.
        if echo "${output}" | grep -qE '\[kernel\] (User Error|failure)|\[wp\] failure'; then
            echo "  ERROR: Frama-C reported failure"
            echo "${output}" | tail -20
            FAIL=$((FAIL + 1))
            return
        fi
        echo "  no proof obligations (no ACSL contracts yet)"
        PASS=$((PASS + 1))
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
echo " Headless WP Verification (FreeRTOS)"
echo "========================================"
echo ""

# ── Tasks (overlay/tasks.c) ──────────────────────────────────────
echo "== Tasks (overlay/tasks.c) =="
echo ""

run_wp "vTaskSwitchContext" \
    "${OVERLAY}/overlay/tasks.c" \
    "vTaskSwitchContext" \
    -wp-model "Typed+Cast" ${EXTRA_ARGS}

echo ""
echo "========================================"
echo " Summary: ${PASS} passed, ${FAIL} failed"
echo "========================================"
exit ${FAIL}
