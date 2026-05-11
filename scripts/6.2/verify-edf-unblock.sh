#!/bin/bash
#
# Verify _Scheduler_EDF_Unblock on the active RTEMS 6.2 port.
#
# This script uses the active overlay in verification/6.2 and includes the
# abstract EDF ready-set model.  It does not use the legacy hand-port stubs.
#
# Usage:
#   verify-edf-unblock.sh
#   verify-edf-unblock.sh --gui
#   verify-edf-unblock.sh -wp -wp-fct _Scheduler_EDF_Unblock -wp-model "Typed+Cast"
#
set -e

if command -v opam >/dev/null 2>&1; then
    eval $(opam env)
fi

FRAMA_C_CMD="frama-c"
if [ "$1" = "--gui" ]; then
    FRAMA_C_CMD="frama-c-gui"
    shift
fi

RTEMS_SRC="${RTEMS_SRC:-/workspace/rtems/src/rtems-6.2-pristine}"
RTEMS_PREFIX="${RTEMS_PREFIX:-/opt/rtems5}"
OVERLAY="${OVERLAY:-/workspace/verification/6.2}"
RTEMS_BUILD_BSP="${RTEMS_BUILD_BSP:-/workspace/rtems/build/amd64/x86_64-rtems5/c/amd64/include}"

SRC="${OVERLAY}/overlay/cpukit/score/src/scheduleredfunblock.c"
MODEL="${OVERLAY}/models/edf_ready_set.h"

[ -f "${SRC}" ] || { echo "missing overlay source: ${SRC}" >&2; exit 1; }
[ -f "${MODEL}" ] || { echo "missing EDF ready model: ${MODEL}" >&2; exit 1; }

echo "=== EDF Unblock (RTEMS 6.2 active port) ==="
${FRAMA_C_CMD} \
    -cpp-command "${RTEMS_PREFIX}/bin/x86_64-rtems5-gcc -C -E \
        -D__FRAAMC__ \
        -D__rtems__ \
        -I${OVERLAY}/overlay/cpukit/include \
        -I${OVERLAY}/models \
        -I${RTEMS_SRC}/cpukit/include \
        -I${RTEMS_SRC}/cpukit/score/cpu/x86_64/include \
        -I${RTEMS_BUILD_BSP} \
        -I${RTEMS_PREFIX}/x86_64-rtems5/include \
        -I${RTEMS_PREFIX}/lib/gcc/x86_64-rtems5/9.3.0/include \
        -I${RTEMS_SRC}/bsps/include \
        -I${RTEMS_SRC}/bsps/x86_64/include \
        -I${RTEMS_SRC}/bsps/x86_64/amd64/include \
        -nostdinc" \
    -machdep gcc_x86_64 -cpp-frama-c-compliant -std c11 \
    -inline-calls "_Scheduler_uniprocessor_Unblock,_Scheduler_uniprocessor_Update_heir_if_preemptible,_Scheduler_uniprocessor_Update_heir" \
    "${SRC}" \
    "$@"
