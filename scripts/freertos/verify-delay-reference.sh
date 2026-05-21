#!/bin/bash
#
# Headless WP run for the standalone reference extraction of
# vTaskDelay and xTaskDelayUntil (verification/freertos/reference/delay.c).
#
# Usage:
#   verify-delay-reference.sh                       # default flags
#   verify-delay-reference.sh -wp-timeout 60        # 60s prover timeout
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

echo "========================================"
echo " WP Verification (FreeRTOS reference)"
echo "========================================"
echo ""
echo "--- vTaskDelay / xTaskDelayUntil (reference) ---"

frama-c \
    -cpp-command "${CPP_CMD}" \
    -machdep "${MACHDEP}" -cpp-frama-c-compliant -std c11 \
    -wp -wp-fct vTaskDelay,xTaskDelayUntil,prvAddCurrentTaskToDelayedList,xTaskResumeAll,vPortYield -wp-model "Typed+Cast" \
    -wp-timeout 10 \
    "$@" \
    "${OVERLAY}/reference/delay.c"
