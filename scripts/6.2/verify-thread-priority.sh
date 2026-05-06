#!/bin/bash
#
# Verify thread priority functions on RTEMS 6.2.
#
# All 7 functions in score/src/threadchangepriority.c require -wp-split
# (without it, WP generates 3 coarse goals per ensures instead of 12
# fine-grained ones — too complex for Qed).
#
# Usage:
#   verify-thread-priority.sh                              # default flags
#   verify-thread-priority.sh --gui                        # frama-c-gui
#   verify-thread-priority.sh -wp-fct _Thread_Priority_apply \
#                             -wp-model "Typed+Cast"      # extra flags
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

echo "=== Thread Priority (RTEMS 6.2) ==="
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
    -inline-calls "_Priority_Node_is_active,_Priority_Extract_non_empty,_Priority_Non_empty_insert,_Priority_Changed,_Thread_Priority_do_perform_actions,_Thread_Priority_apply" \
    'score/src/threadchangepriority.c' \
    "$@"
