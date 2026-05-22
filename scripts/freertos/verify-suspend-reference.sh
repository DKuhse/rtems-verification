#!/bin/bash
#
# Headless WP run for the standalone reference extraction of
# vTaskSuspend (verification/freertos/reference/suspend.c).
#
# Usage:
#   verify-suspend-reference.sh                       # default 30s prover timeout
#   verify-suspend-reference.sh -wp-timeout 60        # 60s prover timeout
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
echo "--- vTaskSuspend (reference) ---"

DEFAULT_TIMEOUT=30
for arg in "$@"; do
    case "${arg}" in
        -wp-timeout|-wp-timeout=*)
            DEFAULT_TIMEOUT=""
            break
            ;;
    esac
done

DEFAULT_ARGS=()
if [ -n "${DEFAULT_TIMEOUT}" ]; then
    DEFAULT_ARGS=(-wp-timeout "${DEFAULT_TIMEOUT}")
fi

frama-c \
    -cpp-command "${CPP_CMD}" \
    -machdep "${MACHDEP}" -cpp-frama-c-compliant -std c11 \
    "${OVERLAY}/reference/suspend.c" \
    -volatile \
    -then-on Volatile \
    -wp -wp-fct vTaskSuspend,vPortYield -wp-model "Typed+Cast" \
    "${DEFAULT_ARGS[@]}" "$@"
