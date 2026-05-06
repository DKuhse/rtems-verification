#!/bin/bash
#
# Parse-check the FreeRTOS overlay headers through Frama-C.
#
# Runs the same preprocessor + Frama-C pipeline as verify-wp-all.sh but
# stops after parsing — verifies the overlay's ACSL annotations are
# syntactically and type-correct without needing any annotated .c file
# to exist yet.
#
# Usage:
#   docker compose run --rm verify-freertos /opt/scripts/freertos/parse-check.sh
#
set -e
eval $(opam env)

FREERTOS_SRC="${FREERTOS_SRC:-/workspace/source/freertos-edf-msp430}"
OVERLAY="${OVERLAY:-/workspace/verification/freertos}"

CHECK=$(mktemp --suffix=.c)
trap 'rm -f "${CHECK}"' EXIT

cat > "${CHECK}" <<'EOF'
#include "FreeRTOS.h"
#include "list.h"
int main(void) { return 0; }
EOF

CPP_CMD="gcc -C -E \
    -D__LARGE_DATA_MODEL__ \
    -I${OVERLAY}/overlay/include \
    -I${OVERLAY}/stubs \
    -I${FREERTOS_SRC}/include \
    -nostdinc \
    -isystem /usr/include \
    -isystem /usr/include/x86_64-linux-gnu \
    -isystem /usr/lib/gcc/x86_64-linux-gnu/11/include"

frama-c \
    -cpp-command "${CPP_CMD}" \
    -machdep gcc_x86_16 \
    -cpp-frama-c-compliant \
    -c11 \
    -kernel-warn-key annot-error=active \
    "${CHECK}"

echo "OK: overlay headers parse cleanly"
