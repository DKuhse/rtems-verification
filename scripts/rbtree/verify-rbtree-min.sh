#!/bin/bash
set -e
eval $(opam env)

FRAMA_C_CMD="frama-c"
if [ "$1" = "--gui" ]; then
    FRAMA_C_CMD="frama-c-gui"
    shift
fi

RTEMS_SRC="${RTEMS_SRC:-/workspace/rtems/src/rtems-6.2-pristine}"
RTEMS_PREFIX="${RTEMS_PREFIX:-/opt/rtems5}"
RBTREE_DIR="${RBTREE_DIR:-/workspace/verification/rbtree}"

echo "=== RBTree Minimum (RTEMS 6.2) ==="
# Verify both _RBTree_Minimum and the structural lemmas (wf_node_valid,
# wf_node_left). The function proof depends on the lemmas; running them
# in the same invocation keeps the "everything was actually proved"
# claim honest.
${FRAMA_C_CMD} \
    -cpp-command "${RTEMS_PREFIX}/bin/x86_64-rtems5-gcc -C -E \
        -I${RBTREE_DIR}/include \
        -I${RTEMS_SRC}/cpukit/include \
        -I${RTEMS_SRC}/cpukit/score/cpu/x86_64/include \
        -I/workspace/rtems/build/amd64/x86_64-rtems5/c/amd64/include \
        -I${RTEMS_PREFIX}/x86_64-rtems5/include \
        -I${RTEMS_PREFIX}/lib/gcc/x86_64-rtems5/9.3.0/include \
        -nostdinc -include ${RBTREE_DIR}/stubs-rbtree.h" \
    -machdep gcc_x86_64 -cpp-frama-c-compliant -c11 \
    -wp -wp-model "Typed+Cast" -wp-split \
    "${RBTREE_DIR}/rbtreemin.c" \
    "$@"
