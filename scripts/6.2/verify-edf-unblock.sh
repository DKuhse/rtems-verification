#!/bin/bash
#
# Verify _Scheduler_EDF_Unblock on RTEMS 6.2.
#
# Usage:
#   verify-edf-unblock.sh                           # default flags
#   verify-edf-unblock.sh --gui                     # frama-c-gui
#   verify-edf-unblock.sh -wp-model "Typed+Cast"    # extra flags
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

cd "${RTEMS_SRC}/cpukit"

echo "=== EDF Unblock (RTEMS 6.2) ==="
${FRAMA_C_CMD} \
    -cpp-command "${RTEMS_PREFIX}/bin/x86_64-rtems5-gcc -C -E \
        -I./include -I./score/cpu/x86_64/include/ \
        -I/workspace/rtems/build/amd64/x86_64-rtems5/c/amd64/include/ \
        -I${RTEMS_PREFIX}/x86_64-rtems5/include \
        -I${RTEMS_PREFIX}/lib/gcc/x86_64-rtems5/9.3.0/include \
        -I${RTEMS_SRC}/bsps/include \
        -I${RTEMS_SRC}/bsps/x86_64/include \
        -I${RTEMS_SRC}/bsps/x86_64/amd64/include \
        -nostdinc -include stubs.h" \
    -machdep gcc_x86_64 -cpp-frama-c-compliant -c11 \
    -inline-calls "_Scheduler_uniprocessor_Unblock,_Scheduler_uniprocessor_Update_heir_if_preemptible,_Scheduler_uniprocessor_Update_heir" \
    'score/src/scheduleredfunblock.c' \
    "$@"
