#!/bin/bash
#
# Verify _Thread_Priority_update() on the active RTEMS 6.2 port.
#
# This is separate from verify-thread-change-priority.sh: that script checks
# the priority aggregation helpers and public add/change/remove wrappers, while
# this one checks the composition step that consumes a queued update and calls
# the scheduler update-priority operation.
#
# Usage:
#   verify-thread-priority-update.sh                 # default proof
#   verify-thread-priority-update.sh --gui           # open in GUI
#   verify-thread-priority-update.sh -wp-prop=foo    # narrow goals
#
set -e

WP_FCTS="${WP_FCTS:-_Thread_Priority_update}"
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

RTEMS_SRC="${RTEMS_SRC:-/workspace/rtems/src/rtems-6.2-pristine}"
RTEMS_PREFIX="${RTEMS_PREFIX:-/opt/rtems5}"
OVERLAY="${OVERLAY:-/workspace/verification/6.2}"
RTEMS_BUILD_BSP="${RTEMS_BUILD_BSP:-/workspace/rtems/build/amd64/x86_64-rtems5/c/amd64/include}"

SRC="${OVERLAY}/overlay/cpukit/score/src/threadchangepriority.c"

[ -f "${SRC}" ] || { echo "missing overlay source: ${SRC}" >&2; exit 1; }

echo "=== Thread Priority Update (RTEMS 6.2 active port) ==="
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
