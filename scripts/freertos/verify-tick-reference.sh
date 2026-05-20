#!/bin/bash
#
# Headless WP run for the standalone reference extraction of
# xTaskIncrementTick (verification/freertos/reference/incrementtick.c).
#
# Usage:
#   verify-tick-reference.sh                       # default flags
#   verify-tick-reference.sh -wp-timeout 60        # 60s prover timeout
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
echo "--- xTaskIncrementTick (reference) ---"

frama-c \
    -cpp-command "${CPP_CMD}" \
    -machdep "${MACHDEP}" -cpp-frama-c-compliant -std c11 \
    -wp -wp-fct xTaskIncrementTick -wp-model "Typed+Cast" \
    "$@" \
    "${OVERLAY}/reference/incrementtick.c"
