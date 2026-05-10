#!/bin/bash
#
# Headless WP run for the per-iteration helpers extracted from
# xTaskIncrementTick (reference/incrementtick.c).
#
# Companion to verify-tick-reference.sh: that script verifies the caller
# (treating the helper's contract as a black-box assumption); this one
# verifies the helper itself satisfies its declared contract. Both must
# pass for the proof to be complete.
#
# Usage:
#   verify-tick-helper.sh                          # default flags
#   verify-tick-helper.sh -wp-timeout 60           # 60s prover timeout
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
    -I${OVERLAY}/overlay/include \
    -I${OVERLAY}/stubs \
    -I${FREERTOS_SRC}/include \
    -nostdinc \
    -isystem /usr/include \
    -isystem /usr/include/x86_64-linux-gnu \
    -isystem /usr/lib/gcc/x86_64-linux-gnu/11/include"

echo "========================================"
echo " WP Verification (FreeRTOS reference)"
echo "========================================"
echo ""
echo "--- xTaskIncrementTick helpers (reference) ---"

frama-c \
    -cpp-command "${CPP_CMD}" \
    -machdep "${MACHDEP}" -cpp-frama-c-compliant -c11 \
    -wp \
    -wp-fct prvDetachUnblockedTaskFromDelayedList \
    -wp-fct prvInsertUnblockedTaskIntoReadyList \
    -wp-fct prvProcessUnblockedTask \
    -wp-model "Typed+Cast" \
    "$@" \
    "${OVERLAY}/reference/incrementtick.c"
