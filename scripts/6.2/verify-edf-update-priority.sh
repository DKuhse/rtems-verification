#!/bin/bash
#
# Verify _Scheduler_EDF_Update_priority on RTEMS 6.2.
#
# Usage:
#   verify-edf-update-priority.sh                           # default flags
#   verify-edf-update-priority.sh --gui                     # frama-c-gui
#   verify-edf-update-priority.sh -wp-model "Typed+Cast"    # extra flags
#
set -e
eval $(opam env)

FRAMA_C_CMD="frama-c"
if [ "$1" = "--gui" ]; then
    FRAMA_C_CMD="frama-c-gui"
    shift
fi

RTEMS_SRC="${RTEMS_SRC:-/workspace/rtems/src/rtems-6.2}"
RTEMS_PREFIX="${RTEMS_PREFIX:-/opt/rtems5}"
OVERLAY="${OVERLAY:-/workspace/verification/6.2}"

SRC="${OVERLAY}/overlay/cpukit/score/src/scheduleredfchangepriority.c"
STUB="${OVERLAY}/stubs/stubs.h"
[ -f "${SRC}" ]  || { echo "missing overlay source: ${SRC}" >&2; exit 1; }
[ -f "${STUB}" ] || { echo "missing stub: ${STUB}" >&2; exit 1; }

echo "=== EDF Update Priority (RTEMS 6.2) ==="
${FRAMA_C_CMD} \
    -cpp-command "${RTEMS_PREFIX}/bin/x86_64-rtems5-gcc -C -E \
        -I${OVERLAY}/overlay/cpukit/include \
        -I${OVERLAY}/stubs \
        -I${RTEMS_SRC}/cpukit/include \
        -I${RTEMS_SRC}/cpukit/score/cpu/x86_64/include \
        -I/workspace/rtems/build/amd64/x86_64-rtems5/c/amd64/include \
        -I${RTEMS_PREFIX}/x86_64-rtems5/include \
        -I${RTEMS_PREFIX}/lib/gcc/x86_64-rtems5/9.3.0/include \
        -I${RTEMS_SRC}/bsps/include \
        -I${RTEMS_SRC}/bsps/x86_64/include \
        -I${RTEMS_SRC}/bsps/x86_64/amd64/include \
        -nostdinc -include ${STUB}" \
    -machdep gcc_x86_64 -cpp-frama-c-compliant -c11 \
    -inline-calls "_Scheduler_uniprocessor_Schedule,_Scheduler_EDF_Get_highest_ready,_Scheduler_uniprocessor_Update_heir_if_preemptible,_Scheduler_uniprocessor_Update_heir" \
    "${SRC}" \
    "$@"
