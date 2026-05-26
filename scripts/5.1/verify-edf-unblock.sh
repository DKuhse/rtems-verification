#!/bin/bash
#
# Verify _Scheduler_EDF_Unblock on the RTEMS 5.1 port.
#
# _Scheduler_Update_heir is verified separately by verify-scheduler-update-heir.sh.
# Here we run the EDF entry point + its EDF inline helpers against their bodies,
# using verified primitive contracts for the heir-update layer.
#
set -e

WP_FCTS="${WP_FCTS:-_Scheduler_EDF_Unblock,_Scheduler_EDF_Get_context,_Scheduler_EDF_Node_downcast,_Scheduler_Node_get_priority,_Thread_Get_priority,_Thread_Scheduler_get_home_node,_Priority_Get_priority}"
# _Scheduler_EDF_Enqueue is verified against its body in the Schedule/Map-Unmap
# slices; here it is used through its contract. Including it in -wp-fct would
# trigger redundant body checks that time out on the _RBTree_Insert_inline
# call (no assigns specification by design).

WP_FCT_DEFAULTS="${WP_FCT_DEFAULTS:--wp -wp-fct ${WP_FCTS} -wp-model Typed+Cast -wp-timeout 30}"

if command -v opam >/dev/null 2>&1; then
    eval $(opam env)
fi

FRAMA_C_CMD="frama-c"
GUI=0
if [ "$1" = "--gui" ]; then
    FRAMA_C_CMD="frama-c-gui"
    GUI=1
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

SRC="${OVERLAY}/overlay/cpukit/score/src/scheduleredfunblock.c"
MODEL="${OVERLAY}/models/edf_ready_set.h"

[ -f "${SRC}" ] || { echo "missing overlay source: ${SRC}" >&2; exit 1; }
[ -f "${MODEL}" ] || { echo "missing EDF ready model: ${MODEL}" >&2; exit 1; }

run_fc() {
    local defaults="$1"; shift
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
        -volatile \
        -then-on Volatile \
        ${defaults} \
        "$@"
}

if [ "${GUI}" = "1" ]; then
    run_fc "${WP_FCT_DEFAULTS}" "$@"
else
    echo "=== EDF Unblock (RTEMS 5.1): function ==="
    run_fc "${WP_FCT_DEFAULTS}" "$@"
fi
