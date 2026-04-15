#!/bin/bash
set -e
eval $(opam env)

FRAMA_C_CMD="frama-c"
if [ "$1" = "--gui" ]; then
    FRAMA_C_CMD="frama-c-gui"
    shift
fi

RTEMS_SRC="${RTEMS_SRC:-/workspace/rtems/src/rtems-5.1}"
RTEMS_PREFIX="${RTEMS_PREFIX:-/opt/rtems5}"

cd "${RTEMS_SRC}/cpukit"

echo "=== EDF Release and Cancel Job ==="
${FRAMA_C_CMD} \
    -cpp-command "${RTEMS_PREFIX}/bin/x86_64-rtems5-gcc -C -E \
        -I./include -I./score/cpu/x86_64/include/ \
        -I../../../build/amd64/x86_64-rtems5/c/amd64/include/ \
        -I${RTEMS_PREFIX}/x86_64-rtems5/include \
        -I${RTEMS_PREFIX}/lib/gcc/x86_64-rtems5/9.3.0/include \
        -nostdinc -include release_cancel_stubs.h" \
    -machdep gcc_x86_64 -cpp-frama-c-compliant -c11 \
    'score/src/scheduleredfreleasejob.c' \
    "$@"
