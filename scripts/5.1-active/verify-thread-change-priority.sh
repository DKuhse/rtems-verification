#!/bin/bash
#
# Verify the UP thread-priority aggregation operations used by EDF
# release/cancel job on the active RTEMS 5.1 port.
#
# The priority RBTree/plain operations remain an intentional abstraction
# boundary for this project. This script targets the priority combinators plus
# the action-list helper, change callback, scheduler-node setter helper, and
# the public thread-priority wrappers.
#
# Usage:
#   verify-thread-change-priority.sh                 # default proof
#   verify-thread-change-priority.sh --gui           # open in GUI
#   verify-thread-change-priority.sh -wp-prop=foo    # narrow goals
#
set -e

WP_FCTS="${WP_FCTS:-_Priority_Actions_add,_Priority_Non_empty_insert,_Priority_Extract_non_empty,_Priority_Changed,_Thread_Set_scheduler_node_priority,_Thread_Priority_action_change,_Thread_Priority_add,_Thread_Priority_changed,_Thread_Priority_remove}"

WP_FCT_DEFAULTS="${WP_FCT_DEFAULTS:--wp -wp-fct ${WP_FCTS} -wp-model Typed+Cast -wp-timeout 30}"

if command -v opam >/dev/null 2>&1; then
    eval $(opam env)
fi

FRAMA_C_CMD="frama-c"
if [ "$1" = "--gui" ]; then
    FRAMA_C_CMD="frama-c-gui"
    shift
fi

C_STD_FLAGS=(-std c11)
if [[ "$(${FRAMA_C_CMD} -version 2>/dev/null)" == 25.* ]]; then
    C_STD_FLAGS=(-c11)
fi

RTEMS_SRC="${RTEMS_SRC:-/workspace/rtems/src/rtems-5.1-pristine}"
RTEMS_PREFIX="${RTEMS_PREFIX:-/opt/rtems5}"
OVERLAY="${OVERLAY:-/workspace/verification/5.1}"
RTEMS_BUILD_BSP="${RTEMS_BUILD_BSP:-/workspace/rtems/build/amd64/x86_64-rtems5/c/amd64/include}"

SRC="${OVERLAY}/overlay/cpukit/score/src/threadchangepriority.c"

[ -f "${SRC}" ] || { echo "missing overlay source: ${SRC}" >&2; exit 1; }

echo "=== Thread Change Priority (RTEMS 5.1 active port) ==="
${FRAMA_C_CMD} \
    -cpp-command "${RTEMS_PREFIX}/bin/x86_64-rtems5-gcc -C -E \
        -D__FRAMAC__ \
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
    -machdep gcc_x86_64 -cpp-frama-c-compliant "${C_STD_FLAGS[@]}" \
    "${SRC}" \
    ${WP_FCT_DEFAULTS} \
    "$@"
