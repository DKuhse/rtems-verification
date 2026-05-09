#!/bin/bash
#
# Soundness test for consistent_membership preservation in the list
# mutator contracts. Runs the __test_cons_mem_soundness function in
# reference/incrementtick.c.
#
# The test has a sanity probe (assert \false) AFTER one uxListRemove
# call. With two consistent_membership preconditions in scope, the
# probe currently proves Valid (model is unsound). Without
# consistent_membership preservation in uxListRemove's contract,
# the probe Times out (model is sound).
#
# Use to investigate the soundness issue, try alternative formulations,
# or test memory-model choices.
#
# Usage:
#   verify-cons-mem-test.sh                       # default flags
#   verify-cons-mem-test.sh -wp-timeout 60        # 60s prover timeout
#   verify-cons-mem-test.sh -wp-prover=qed -wp-print  # show proof obligation
#
set -e
eval $(opam env)

FREERTOS_SRC="${FREERTOS_SRC:-/workspace/source/freertos-edf-msp430}"
OVERLAY="${OVERLAY:-/workspace/verification/freertos}"

MACHDEP="gcc_x86_16"

CPP_CMD="gcc -C -E \
    -D__LARGE_DATA_MODEL__ \
    -D__FRAMAC__ \
    -DEDF_SCHEDULER=1 \
    -DSANITY_PROBE=1 \
    -I${OVERLAY}/overlay/include \
    -I${OVERLAY}/stubs \
    -I${FREERTOS_SRC}/include \
    -nostdinc \
    -isystem /usr/include \
    -isystem /usr/include/x86_64-linux-gnu \
    -isystem /usr/lib/gcc/x86_64-linux-gnu/11/include"

echo "========================================"
echo " WP Verification (cons_mem soundness test)"
echo "========================================"
echo ""
echo "--- __test_cons_mem_soundness ---"
echo ""
echo "Expected:"
echo "  - With consistent_membership preservation in uxListRemove:"
echo "      probe proves \\false (Valid) → UNSOUND."
echo "  - Without:"
echo "      probe Times out → sound."
echo ""

frama-c \
    -cpp-command "${CPP_CMD}" \
    -machdep "${MACHDEP}" -cpp-frama-c-compliant -c11 \
    -wp -wp-fct __test_cons_mem_soundness -wp-model "Typed+Cast" \
    "$@" \
    "${OVERLAY}/reference/incrementtick.c"
