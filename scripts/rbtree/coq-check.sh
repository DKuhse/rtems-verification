#!/bin/bash
# Compile a Why3-generated .v file with the right library path.
# Usage: coq-check.sh <file.v> [more files...]
#
# Fast iteration loop for Coq proof development — skips WP entirely,
# just runs coqc with the Why3 Coq library on the load path. When the
# file compiles clean, re-run frama-c with -wp-prover alt-ergo,coq to
# let WP pick up the proof via its session cache.
set -e
eval $(opam env)
# WP-generated files use bare `Require Import BuiltIn.` (not qualified),
# so use -R (visible without prefix) rather than -Q.
exec coqc -R /root/.opam/4.14.1/lib/why3/coq Why3 "$@"
