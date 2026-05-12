#!/bin/bash
#
# Verify _Scheduler_EDF_Schedule on the active RTEMS 6.2 port.
#
# Two WP passes:
#   1. function goals for _Scheduler_EDF_Schedule + trivially-verifiable helpers
#   2. the EDF property lemma in the model
#
set -e

WP_FCTS="${WP_FCTS:-_Scheduler_EDF_Schedule,_Scheduler_EDF_Get_context,_Thread_Is_ready}"

WP_FCT_DEFAULTS="${WP_FCT_DEFAULTS:--wp -wp-fct ${WP_FCTS} -wp-model Typed+Cast -wp-timeout 30}"
WP_LEMMA_DEFAULTS="${WP_LEMMA_DEFAULTS:--wp -wp-prop=@lemma -wp-model Typed+Cast -wp-timeout 30}"

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

RTEMS_SRC="${RTEMS_SRC:-/workspace/rtems/src/rtems-6.2-pristine}"
RTEMS_PREFIX="${RTEMS_PREFIX:-/opt/rtems5}"
OVERLAY="${OVERLAY:-/workspace/verification/6.2}"
RTEMS_BUILD_BSP="${RTEMS_BUILD_BSP:-/workspace/rtems/build/amd64/x86_64-rtems5/c/amd64/include}"

SRC="${OVERLAY}/overlay/cpukit/score/src/scheduleredfschedule.c"
MODEL="${OVERLAY}/models/edf_ready_set.h"

[ -f "${SRC}" ] || { echo "missing overlay source: ${SRC}" >&2; exit 1; }
[ -f "${MODEL}" ] || { echo "missing EDF ready model: ${MODEL}" >&2; exit 1; }

run_fc() {
    local defaults="$1"; shift
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
        -machdep gcc_x86_64 -cpp-frama-c-compliant "${C_STD_FLAGS[@]}" \
        -inline-calls "_Scheduler_uniprocessor_Schedule,_Scheduler_uniprocessor_Update_heir_if_preemptible" \
        "${SRC}" \
        ${defaults} \
        "$@"
}

if [ "${GUI}" = "1" ]; then
    run_fc "${WP_FCT_DEFAULTS}" "$@"
else
    echo "=== EDF Schedule (RTEMS 6.2 active port): function ==="
    run_fc "${WP_FCT_DEFAULTS}" "$@"
    echo "=== EDF Schedule (RTEMS 6.2 active port): model lemma ==="
    run_fc "${WP_LEMMA_DEFAULTS}" "$@"
fi
