#!/bin/bash
#
# Composite runner for the RTEMS 5.1 verification scripts.
#
# Runs every documented verify-*.sh in this directory in turn, parses each WP
# pass's "Proved goals: X / Y" summary, and prints a per-script table
# plus an aggregate total. Exits non-zero if any script left goals
# unproved (or failed to emit a WP summary at all).
#
# Usage:
#   verify-all.sh                  # run all 5.1 scripts (WP cache on)
#   verify-all.sh --no-cache       # disable the WP proof cache
#   verify-all.sh -j 8             # run up to 8 scripts in parallel (default nproc/2)
#   verify-all.sh -- -wp-timeout 60   # forward extra args to every script
#
# Parallelism:
#   Scripts are launched concurrently up to -j JOBS at a time. Their stdout
#   is buffered to per-script files and replayed in the original SCRIPTS
#   order so the report is deterministic. Note that each WP invocation
#   itself spawns provers in parallel (-wp-par); over-subscribing -j
#   can thrash the CPU. The default of nproc/2 (min 1) leaves headroom
#   for WP's own prover parallelism.
#
# WP proof cache:
#   FRAMAC_WP_CACHE / FRAMAC_WP_CACHEDIR control WP's per-goal proof cache.
#   This script exports both with sensible defaults so the second run of
#   a script only re-proves goals whose VCs changed. Override either env
#   var, or pass --no-cache, to opt out.
#
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

USE_CACHE=1
JOBS=$(( $(nproc 2>/dev/null || echo 2) / 2 ))
[ "${JOBS}" -lt 1 ] && JOBS=1
EXTRA_ARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --no-cache) USE_CACHE=0; shift ;;
        -j)         JOBS="$2"; shift 2 ;;
        -j*)        JOBS="${1#-j}"; shift ;;
        --jobs=*)   JOBS="${1#--jobs=}"; shift ;;
        --) shift; EXTRA_ARGS=("$@"); break ;;
        *) EXTRA_ARGS+=("$1"); shift ;;
    esac
done

if ! [[ "${JOBS}" =~ ^[0-9]+$ ]] || [ "${JOBS}" -lt 1 ]; then
    echo "verify-all.sh: -j must be a positive integer (got '${JOBS}')" >&2
    exit 2
fi

if [ "${USE_CACHE}" = "1" ]; then
    : "${FRAMAC_WP_CACHE:=update}"
    : "${FRAMAC_WP_CACHEDIR:=${HOME}/.frama-c-wp-cache}"
    mkdir -p "${FRAMAC_WP_CACHEDIR}"
    export FRAMAC_WP_CACHE FRAMAC_WP_CACHEDIR
    echo "WP cache: mode=${FRAMAC_WP_CACHE} dir=${FRAMAC_WP_CACHEDIR}"
else
    unset FRAMAC_WP_CACHE FRAMAC_WP_CACHEDIR
    echo "WP cache: disabled"
fi

# Documented 5.1 scripts. Keep this list explicit so temporary or
# exploratory scripts are not pulled into the checkpoint runner by accident.
SCRIPTS=(
    verify-scheduler-update-heir.sh
    verify-edf-initialize.sh
    verify-edf-node-initialize.sh
    verify-edf-map-unmap.sh
    verify-edf-schedule.sh
    verify-edf-block.sh
    verify-edf-unblock.sh
    verify-edf-yield.sh
    verify-edf-update-priority.sh
    verify-edf-release-cancel.sh
    verify-scheduler-update-priority.sh
    verify-scheduler-release-job.sh
    verify-scheduler-cancel-job.sh
    verify-thread-change-priority.sh
    verify-thread-priority-update.sh
    verify-ratemon-release-job.sh
    verify-ratemon-cancel.sh
)

TOTAL_PROVED=0
TOTAL_GOALS=0
SCRIPTS_OK=0
SCRIPTS_BAD=0
declare -a REPORT_LINES

# Buffer per-script output to a temp dir so parallel jobs don't interleave.
TMPDIR_RUN="$(mktemp -d "${TMPDIR:-/tmp}/verify-all.XXXXXX")"
trap 'rm -rf "${TMPDIR_RUN}"' EXIT

echo "Parallelism: -j ${JOBS} (per-script output buffered, printed in order)"

# Launch jobs with a simple semaphore: keep at most ${JOBS} background
# children alive at any moment. `wait -n` blocks until one finishes.
running=0
for idx in "${!SCRIPTS[@]}"; do
    script="${SCRIPTS[$idx]}"
    path="${HERE}/${script}"
    out="${TMPDIR_RUN}/${idx}.out"
    rcfile="${TMPDIR_RUN}/${idx}.rc"

    if [ ! -x "${path}" ]; then
        : > "${out}"
        echo "MISSING" > "${rcfile}"
        continue
    fi

    while [ "${running}" -ge "${JOBS}" ]; do
        wait -n
        running=$((running - 1))
    done

    (
        "${path}" "${EXTRA_ARGS[@]}" >"${out}" 2>&1
        echo "$?" > "${rcfile}"
    ) &
    running=$((running + 1))
done

wait

for idx in "${!SCRIPTS[@]}"; do
    script="${SCRIPTS[$idx]}"
    out="${TMPDIR_RUN}/${idx}.out"
    rcfile="${TMPDIR_RUN}/${idx}.rc"
    rc_raw="$(cat "${rcfile}" 2>/dev/null || echo MISSING)"

    if [ "${rc_raw}" = "MISSING" ]; then
        echo ">>> SKIP ${script} (missing or not executable)"
        REPORT_LINES+=("$(printf '  %-40s  %s' "${script}" 'SKIPPED (not found)')")
        SCRIPTS_BAD=$((SCRIPTS_BAD + 1))
        continue
    fi
    rc="${rc_raw}"

    echo
    echo "============================================================"
    echo ">>> ${script}"
    echo "============================================================"
    cat "${out}"

    script_proved=0
    script_goals=0
    pass_count=0
    while IFS= read -r line; do
        if [[ "${line}" =~ Proved\ goals:[[:space:]]*([0-9]+)[[:space:]]*/[[:space:]]*([0-9]+) ]]; then
            script_proved=$((script_proved + ${BASH_REMATCH[1]}))
            script_goals=$((script_goals + ${BASH_REMATCH[2]}))
            pass_count=$((pass_count + 1))
        fi
    done < <(grep -E '^\[wp\] Proved goals:' "${out}")

    if [ "${pass_count}" = "0" ]; then
        status="NO WP SUMMARY (exit ${rc})"
        SCRIPTS_BAD=$((SCRIPTS_BAD + 1))
    elif [ "${script_proved}" = "${script_goals}" ] && [ "${rc}" = "0" ]; then
        status="OK  ${script_proved}/${script_goals} (${pass_count} pass$([ ${pass_count} -gt 1 ] && echo es))"
        SCRIPTS_OK=$((SCRIPTS_OK + 1))
    else
        status="FAIL ${script_proved}/${script_goals} (exit ${rc})"
        SCRIPTS_BAD=$((SCRIPTS_BAD + 1))
    fi

    TOTAL_PROVED=$((TOTAL_PROVED + script_proved))
    TOTAL_GOALS=$((TOTAL_GOALS + script_goals))
    REPORT_LINES+=("$(printf '  %-40s  %s' "${script}" "${status}")")
done

echo
echo "============================================================"
echo " Composite verification summary"
echo "============================================================"
for line in "${REPORT_LINES[@]}"; do
    echo "${line}"
done
echo "------------------------------------------------------------"
printf '  %-40s  %s\n' "TOTAL" "${TOTAL_PROVED}/${TOTAL_GOALS} goals proved"
printf '  %-40s  %d ok, %d with problems\n' "scripts" "${SCRIPTS_OK}" "${SCRIPTS_BAD}"
echo "============================================================"

if [ "${SCRIPTS_BAD}" -gt 0 ] || [ "${TOTAL_PROVED}" != "${TOTAL_GOALS}" ]; then
    exit 1
fi
exit 0
