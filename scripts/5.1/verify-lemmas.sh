#!/bin/bash
#
# Prove every ACSL lemma declared in the RTEMS 5.1 verification tree
# in one WP pass.
#
# Lemma locations (covered transitively via the two parsed .c files):
#   - models/edf_ready_set.h            (EDF ready-set preservation lemmas,
#                                        incl. edf_ready_extract_non_empty_if_other_member
#                                        which has no 6.2 counterpart)
#   - models/edf_property.h             (EDF earliest-deadline lemmas)
#   - models/priority_aggregation.h     (priority-min aggregation lemmas)
#   - overlay/.../priorityimpl.h        (priority purity lemmas)
#   - overlay/.../threadchangepriority.c (thread-priority purity lemma)
#
# threadchangepriority.c transitively pulls in priorityimpl.h ->
# priority_aggregation.h. scheduleredfblock.c transitively pulls in
# scheduleredf.h -> edf_ready_set.h + edf_property.h, and (via
# schedulerimpl.h) priorityimpl.h. Parsing both translation units
# brings every lemma into scope for a single `-wp-prop=@lemma` run.
#
set -e

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

RTEMS_SRC="${RTEMS_SRC:-/workspace/rtems/src/rtems-5.1-pristine}"
RTEMS_PREFIX="${RTEMS_PREFIX:-/opt/rtems5}"
OVERLAY="${OVERLAY:-/workspace/verification/5.1}"
RTEMS_BUILD_BSP="${RTEMS_BUILD_BSP:-/workspace/rtems/build/amd64/x86_64-rtems5/c/amd64/include}"

SRC_EDF="${OVERLAY}/overlay/cpukit/score/src/scheduleredfblock.c"
SRC_TCP="${OVERLAY}/overlay/cpukit/score/src/threadchangepriority.c"

[ -f "${SRC_EDF}" ] || { echo "missing overlay source: ${SRC_EDF}" >&2; exit 1; }
[ -f "${SRC_TCP}" ] || { echo "missing overlay source: ${SRC_TCP}" >&2; exit 1; }

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
        "${SRC_EDF}" "${SRC_TCP}" \
        ${defaults} \
        "$@"
}

if [ "${GUI}" = "1" ]; then
    run_fc "${WP_LEMMA_DEFAULTS}" "$@"
else
    echo "=== All lemmas (RTEMS 5.1) ==="
    run_fc "${WP_LEMMA_DEFAULTS}" "$@"
fi
