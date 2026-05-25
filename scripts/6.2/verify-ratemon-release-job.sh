#!/bin/bash
#
# Verify the rate-monotonic release-job composition in the active RTEMS 6.2
# ratemonperiod.c overlay.
#
# Usage:
#   verify-ratemon-release-job.sh                 # default proof
#   verify-ratemon-release-job.sh --gui           # open in GUI
#   verify-ratemon-release-job.sh -wp-prop=foo    # narrow goals
#
set -e

WP_FCTS="${WP_FCTS:-_Rate_monotonic_Release_job}"

WP_FCT_DEFAULTS="${WP_FCT_DEFAULTS:--wp -wp-fct ${WP_FCTS} -wp-model Typed+Cast -wp-timeout 30}"
WP_LEMMA_DEFAULTS="${WP_LEMMA_DEFAULTS:--wp -wp-prop=@lemma -wp-model Typed+Cast -wp-timeout 30}"
INLINE_CALLS="${INLINE_CALLS:-}"
INLINE_ARGS=()
if [ -n "${INLINE_CALLS}" ]; then
    INLINE_ARGS=(-inline-calls "${INLINE_CALLS}")
fi

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

SRC="${OVERLAY}/overlay/cpukit/rtems/src/ratemonperiod.c"
EDF_RELEASE_SRC="${OVERLAY}/overlay/cpukit/score/src/scheduleredfreleasejob.c"

[ -f "${SRC}" ] || { echo "missing overlay source: ${SRC}" >&2; exit 1; }
[ -f "${EDF_RELEASE_SRC}" ] || {
    echo "missing EDF release source: ${EDF_RELEASE_SRC}" >&2
    exit 1
}

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
        "${INLINE_ARGS[@]}" \
        "${EDF_RELEASE_SRC}" \
        "${SRC}" \
        -volatile \
        -then-on Volatile \
        ${defaults} \
        "$@"
}

if [ "${GUI}" = "1" ]; then
    run_fc "${WP_FCT_DEFAULTS}" "$@"
else
    echo "=== Rate-monotonic Release Job (RTEMS 6.2 active port): function ==="
    run_fc "${WP_FCT_DEFAULTS}" "$@"
    echo "=== Rate-monotonic Release Job (RTEMS 6.2 active port): lemma ==="
    run_fc "${WP_LEMMA_DEFAULTS}" "$@"
fi
