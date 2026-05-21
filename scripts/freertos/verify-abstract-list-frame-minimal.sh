#!/bin/bash
#
# Minimal repro for abstract list predicate framing through a pointer write.
#
# Expected result:
#   - write_through_parameter proves the concrete field-frame assertions,
#     then times out on In{Pre}<->In and ListInv reassembly.
#
set -e
eval $(opam env)

OVERLAY="${OVERLAY:-/workspace/verification/freertos}"
MACHDEP="gcc_x86_16"

frama-c \
    -machdep "${MACHDEP}" -cpp-frama-c-compliant -std c11 \
    -wp -wp-fct write_through_parameter \
    -wp-model "Typed+Cast" \
    -wp-timeout 5 \
    "$@" \
    "${OVERLAY}/reference/abstract_list_frame_minimal.c"
