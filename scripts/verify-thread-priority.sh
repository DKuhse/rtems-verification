#!/bin/bash
set -e
eval $(opam env)

FRAMA_C_CMD="frama-c"
if [ "$1" = "--gui" ]; then
    FRAMA_C_CMD="frama-c-gui"
    shift
fi

RTEMS_HOME="${RTEMS_HOME:-$HOME/RTEMS}"
RTEMS_PREFIX="${RTEMS_HOME}/rtems_x86_64"
RTEMS_SRC="${RTEMS_HOME}/src/rtems-5.1"

cd "${RTEMS_SRC}/cpukit"

echo "=== Thread Priority ==="
${FRAMA_C_CMD} \
    -cpp-command "${RTEMS_PREFIX}/bin/x86_64-rtems5-gcc -C -E \
        -I./include -I./score/cpu/x86_64/include/ \
        -I../../../build/amd64/x86_64-rtems5/c/amd64/include/ \
        -I${RTEMS_PREFIX}/x86_64-rtems5/include \
        -I${RTEMS_PREFIX}/lib/gcc/x86_64-rtems5/9.3.0/include \
        -nostdinc -include stubs.h" \
    -machdep gcc_x86_64 -cpp-frama-c-compliant -c11 \
    -inline-calls '_Priority_Node_is_active,_Priority_Extract_non_empty,_Priority_Non_empty_insert,_Priority_Changed' \
    'score/src/threadchangepriority.c' \
    "$@"
