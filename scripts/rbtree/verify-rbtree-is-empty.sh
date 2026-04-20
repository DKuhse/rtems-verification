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

echo "=== RBTree Is_empty (RTEMS 6.2) ==="
# _RBTree_Is_empty is static inline in rbtree.h. It is visible in the
# rbtreemin.c TU (via rbtreeimpl.h -> rbtree.h) so WP can target it
# with -wp-fct even though rbtreemin.c does not call it.
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
    -wp-fct _RBTree_Is_empty \
    "${RBTREE_DIR}/rbtreemin.c" \
    "$@"
