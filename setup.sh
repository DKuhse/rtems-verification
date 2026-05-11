#!/bin/bash
#
# Downloads RTEMS 5.1 + 6.2 sources, builds the AMD64 BSP (via Docker),
# then applies Frama-C patches and copies verification files for 5.1.
# RTEMS 6.2 uses the overlay model and needs no in-tree edits.
#
# Prerequisites:
#   - curl, tar (with xz support)
#   - Docker image built: docker compose build
#
# This script is idempotent — it skips steps that are already done.
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RTEMS_DIR="${SCRIPT_DIR}/rtems"
RTEMS_SRC="${RTEMS_DIR}/src/rtems-5.1"
RTEMS_SRC_PRISTINE="${RTEMS_DIR}/src/rtems-5.1-pristine"
RTEMS_BUILD="${RTEMS_DIR}/build/amd64"
RTEMS_TARBALL="${RTEMS_DIR}/rtems-5.1.tar.xz"
RTEMS_URL="https://ftp.rtems.org/pub/rtems/releases/5/5.1/sources/rtems-5.1.tar.xz"
RTEMS62_SRC="${RTEMS_DIR}/src/rtems-6.2"
RTEMS62_SRC_PRISTINE="${RTEMS_DIR}/src/rtems-6.2-pristine"
RTEMS62_TARBALL="${RTEMS_DIR}/rtems-6.2.tar.xz"
RTEMS62_URL="https://ftp.rtems.org/pub/rtems/releases/6/6.2/sources/rtems-6.2.tar.xz"
VERIFY_DIR="${SCRIPT_DIR}/Formally-Verifying-Implementations-of-EDF-Scheduler-in-RTEMS"

# ── Step 1: Download RTEMS source ──────────────────────────────────
mkdir -p "${RTEMS_DIR}/src" "${RTEMS_BUILD}"

if [ ! -f "${RTEMS_TARBALL}" ]; then
    echo "Downloading RTEMS 5.1 source..."
    curl -fSL -o "${RTEMS_TARBALL}" "${RTEMS_URL}"
else
    echo "RTEMS 5.1 tarball already downloaded."
fi

if [ ! -f "${RTEMS62_TARBALL}" ]; then
    echo "Downloading RTEMS 6.2 source..."
    curl -fSL -o "${RTEMS62_TARBALL}" "${RTEMS62_URL}"
else
    echo "RTEMS 6.2 tarball already downloaded."
fi

# ── Step 2: Extract ────────────────────────────────────────────────
if [ ! -d "${RTEMS_SRC}/cpukit" ]; then
    echo "Extracting RTEMS 5.1 source..."
    tar xJf "${RTEMS_TARBALL}" -C "${RTEMS_DIR}/src"
else
    echo "RTEMS 5.1 source already extracted."
fi

if [ ! -d "${RTEMS62_SRC}/cpukit" ]; then
    echo "Extracting RTEMS 6.2 source..."
    tar xJf "${RTEMS62_TARBALL}" -C "${RTEMS_DIR}/src"
else
    echo "RTEMS 6.2 source already extracted."
fi

# Pristine copies (untouched by patches/overwrites) for reference/diffing.
extract_pristine() {
    local tarball="$1" dest="$2" top="$3"
    if [ ! -f "${tarball}" ]; then return 0; fi
    if [ -d "${dest}/cpukit" ]; then
        echo "Pristine ${top} source already extracted."
        return 0
    fi
    echo "Extracting pristine ${top} source to $(basename "${dest}")/..."
    local tmpdir
    tmpdir="$(mktemp -d -p "${RTEMS_DIR}/src")"
    tar xJf "${tarball}" -C "${tmpdir}"
    mv "${tmpdir}/${top}" "${dest}"
    rmdir "${tmpdir}"
}

extract_pristine "${RTEMS_TARBALL}"   "${RTEMS_SRC_PRISTINE}"   "rtems-5.1"
extract_pristine "${RTEMS62_TARBALL}" "${RTEMS62_SRC_PRISTINE}" "rtems-6.2"

# ── Step 3: Build BSP via Docker (before patching) ────────────────
# The BSP build needs the original unpatched source. Patches and
# verification files are applied afterward for Frama-C only.
if [ ! -f "${RTEMS_BUILD}/Makefile" ]; then
    echo "Building AMD64 BSP via Docker (this may take 10-20 minutes)..."
    docker compose run --rm toolchain bash -c '
        cd /workspace/rtems/build/amd64 &&
        /workspace/rtems/src/rtems-5.1/configure \
            --prefix=/opt/rtems5 \
            --enable-maintainer-mode \
            --target=x86_64-rtems5 \
            --enable-rtemsbsp=amd64 &&
        make &&
        make install
    '
    echo "BSP build complete."
else
    echo "BSP already built."
fi

# ── Step 4: Apply Frama-C compatibility patches (RTEMS 5.1) ───────
echo "Applying Frama-C compatibility patches (5.1)..."

# 4a. Comment out #include <limits.h> in scheduleredf.h
sed -i 's|^#include <limits\.h>|/* #include <limits.h> */ /* disabled for Frama-C */|' \
    "${RTEMS_SRC}/cpukit/include/rtems/score/scheduleredf.h"

# 4b. Comment out line 883 in thread.h
sed -i '883 s|^|// |' \
    "${RTEMS_SRC}/cpukit/include/rtems/score/thread.h"

# 4c. Replace flexible array _Per_CPU_Information[] with fixed [1U]
sed -i 's|Per_CPU_Control_envelope _Per_CPU_Information\[\]|Per_CPU_Control_envelope _Per_CPU_Information[1U]|' \
    "${RTEMS_SRC}/cpukit/include/rtems/score/percpu.h"

echo "Patches (5.1) applied."

# The old RTEMS 6.2 overlay hand-port is retained under
# legacy/rtems-6.2-hand-port/ as reference material. The active 6.2 effort is
# documented in verification/6.2/EDF_RBTREE_ABSTRACTION_PLAN.md and should
# rebuild the proof around an abstract RBTree contract layer.

# ── Step 5: Copy verification files into RTEMS source tree ────────
echo "Copying verification files (5.1)..."

# Stubs -> cpukit/
cp "${VERIFY_DIR}/stubs.h" "${RTEMS_SRC}/cpukit/"
cp "${VERIFY_DIR}/release_cancel_stubs.h" "${RTEMS_SRC}/cpukit/"

# Annotated headers -> cpukit/include/rtems/score/
cp "${VERIFY_DIR}/priorityimpl.h" "${RTEMS_SRC}/cpukit/include/rtems/score/"
cp "${VERIFY_DIR}/scheduleredfimpl.h" "${RTEMS_SRC}/cpukit/include/rtems/score/"
cp "${VERIFY_DIR}/schedulerimpl.h" "${RTEMS_SRC}/cpukit/include/rtems/score/"

# Annotated C sources -> cpukit/score/src/
cp "${VERIFY_DIR}/scheduleredfchangepriority.c" "${RTEMS_SRC}/cpukit/score/src/"
cp "${VERIFY_DIR}/scheduleredfreleasejob.c" "${RTEMS_SRC}/cpukit/score/src/"
cp "${VERIFY_DIR}/scheduleredfunblock.c" "${RTEMS_SRC}/cpukit/score/src/"
cp "${VERIFY_DIR}/threadchangepriority.c" "${RTEMS_SRC}/cpukit/score/src/"

echo "Verification files copied (5.1)."

# ── Done ──────────────────────────────────────────────────────────
echo ""
echo "Setup complete."
echo "  Modified (annotated) 5.1 source: rtems/src/rtems-5.1/"
echo "  Pristine             5.1 source: rtems/src/rtems-5.1-pristine/"
echo "  Pristine             6.2 source: rtems/src/rtems-6.2/"
echo "  Pristine reference   6.2 source: rtems/src/rtems-6.2-pristine/"
echo "  Legacy 6.2 hand-port:             legacy/rtems-6.2-hand-port/"
echo "  Active 6.2 scaffold:              verification/6.2/, scripts/6.2/"
echo "  Active 6.2 plan:                  verification/6.2/EDF_RBTREE_ABSTRACTION_PLAN.md"
echo ""
echo "  Diff modified vs pristine (5.1): diff -ruN rtems/src/rtems-5.1-pristine/ rtems/src/rtems-5.1/"
echo "  Diff legacy overlay vs pristine (6.2): diff -ruN rtems/src/rtems-6.2-pristine/cpukit/ legacy/rtems-6.2-hand-port/verification/6.2/overlay/cpukit/"
echo ""
echo "Usage (RTEMS 5.1):"
echo "  docker compose run --rm verify verify-wp-all.sh -wp-model 'Typed+Cast'"
echo "  docker compose run --rm verify verify-edf-update-priority.sh -wp -wp-fct _Scheduler_EDF_Update_priority -wp-model 'Typed+Cast'"
echo ""
echo "Usage (legacy RTEMS 6.2 hand-port):"
echo "  docker compose run --rm verify-6.2"
echo "  docker compose run --rm verify-6.2 /opt/scripts/6.2/verify-edf-update-priority.sh -wp -wp-fct _Scheduler_EDF_Update_priority -wp-model 'Typed+Cast'"
echo ""
echo "Usage (active RTEMS 6.2 abstract RBTree port):"
echo "  docker compose run --rm verify-6.2-active"
echo "  docker compose run --rm verify-6.2-active /opt/scripts/6.2/verify-edf-unblock.sh -wp -wp-fct _Scheduler_EDF_Unblock -wp-model 'Typed+Cast'"
echo ""
echo "  xhost +local:docker && docker compose run --rm gui"
