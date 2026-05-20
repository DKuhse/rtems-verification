#!/bin/bash
#
# Launch frama-c-gui on the FreeRTOS EDF port with the same CPP/machdep
# flags as verify-wp-all.sh. Defaults to overlay/tasks.c (the full
# tasks.c overlay); pass an alternate source on the command line.
#
# Usage:
#   gui-wp.sh                                          # default: overlay/tasks.c
#   gui-wp.sh /workspace/.../other.c                   # different source
#   gui-wp.sh -wp-fct foo /workspace/.../x.c           # extra flags + source
#
# A smaller standalone reference for vTaskSwitchContext alone lives
# at reference/taskswitchcontext.c — pass it explicitly if useful.
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

if [ "$#" -eq 0 ]; then
    set -- "${OVERLAY}/overlay/tasks.c"
fi

exec frama-c-gui \
    -cpp-command "${CPP_CMD}" \
    -machdep "${MACHDEP}" -cpp-frama-c-compliant -std c11 \
    -wp -wp-model "Typed+Cast" \
    "$@"
