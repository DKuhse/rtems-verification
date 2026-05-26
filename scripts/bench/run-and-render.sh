#!/bin/bash
# Drives the per-function WP benchmark end-to-end:
#   1. inside one container, runs inside-container.sh twice (parallel, then serial)
#   2. saves per-target logs and the parsed RESULT lines under
#      scripts/bench/results/
#   3. renders four LaTeX tables to stdout via render-table.py
#
# Run from the repo root (the directory that contains rtems/, scripts/,
# verification/, source/). Requires the rtems-edf-toolchain-fc32 image.
#
# Usage:
#   scripts/bench/run-and-render.sh             # default: parallel pass only
#   PASS=serial   scripts/bench/run-and-render.sh   # serial pass only
#   PASS=both     scripts/bench/run-and-render.sh   # both passes sequentially
#   RENDER_ONLY=1 scripts/bench/run-and-render.sh   # skip docker, just render
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BENCH_DIR="${REPO_ROOT}/scripts/bench"
RESULTS_DIR="${BENCH_DIR}/results"
mkdir -p "${RESULTS_DIR}" "${RESULTS_DIR}/logs-parallel" "${RESULTS_DIR}/logs-serial"

IMAGE="${IMAGE:-rtems-edf-toolchain-fc32}"
PASS="${PASS:-parallel}"

run_pass() {
    local pass_name="$1" wp_par_env="$2"
    local out_file="${RESULTS_DIR}/run-${pass_name}.out"
    local err_file="${RESULTS_DIR}/run-${pass_name}.err"
    local logs_subdir="${RESULTS_DIR}/logs-${pass_name}"

    echo "[bench] === ${pass_name} pass (${wp_par_env:-default WP_PAR}) ===" >&2

    # stdout (RESULT lines + verbose Frama-C log) → out_file
    # stderr (progress bar lines)                  → err_file AND user terminal
    # The process substitution keeps stderr live for the user while still
    # capturing it on disk for postmortem.
    docker run --rm \
        -v "${REPO_ROOT}/rtems":/workspace/rtems \
        -v "${REPO_ROOT}/scripts":/opt/scripts \
        -v "${REPO_ROOT}/verification":/workspace/verification \
        -v "${REPO_ROOT}/source":/workspace/source \
        -v "${BENCH_DIR}":/opt/bench \
        -v "${logs_subdir}":/tmp/wp-logs \
        -e "${wp_par_env}" \
        -e "WP_TIMEOUT=${WP_TIMEOUT:-120}" \
        "${IMAGE}" \
        bash /opt/bench/inside-container.sh \
        >"${out_file}" 2> >(tee "${err_file}" >&2)

    grep '^RESULT' "${out_file}" > "${RESULTS_DIR}/results-${pass_name}.txt"
    echo "[bench] -> ${RESULTS_DIR}/results-${pass_name}.txt ($(wc -l < "${RESULTS_DIR}/results-${pass_name}.txt") rows)" >&2
}

if [ -z "${RENDER_ONLY:-}" ]; then
    case "${PASS}" in
        parallel) run_pass parallel "WP_PAR=" ;;
        serial)   run_pass serial   "WP_PAR=1" ;;
        both)
            run_pass parallel "WP_PAR="
            run_pass serial   "WP_PAR=1"
            ;;
        *) echo "unknown PASS=${PASS}" >&2; exit 2 ;;
    esac
fi

echo "[bench] === LaTeX ===" >&2
python3 "${BENCH_DIR}/render-table.py" \
    --parallel "${RESULTS_DIR}/results-parallel.txt" \
    --serial   "${RESULTS_DIR}/results-serial.txt"
