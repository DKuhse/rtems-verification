#!/bin/bash
#
# Downloads RTEMS 5.1 source, builds the AMD64 BSP (via Docker), then
# applies Frama-C patches and copies verification files.
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
RTEMS_BUILD="${RTEMS_DIR}/build/amd64"
RTEMS_TARBALL="${RTEMS_DIR}/rtems-5.1.tar.xz"
RTEMS_URL="https://ftp.rtems.org/pub/rtems/releases/5/5.1/sources/rtems-5.1.tar.xz"
VERIFY_DIR="${SCRIPT_DIR}/Formally-Verifying-Implementations-of-EDF-Scheduler-in-RTEMS"

# ── Step 1: Download RTEMS source ──────────────────────────────────
mkdir -p "${RTEMS_DIR}/src" "${RTEMS_BUILD}"

if [ ! -f "${RTEMS_TARBALL}" ]; then
    echo "Downloading RTEMS 5.1 source..."
    curl -fSL -o "${RTEMS_TARBALL}" "${RTEMS_URL}"
else
    echo "RTEMS 5.1 tarball already downloaded."
fi

# ── Step 2: Extract ────────────────────────────────────────────────
if [ ! -d "${RTEMS_SRC}/cpukit" ]; then
    echo "Extracting RTEMS 5.1 source..."
    tar xJf "${RTEMS_TARBALL}" -C "${RTEMS_DIR}/src"
else
    echo "RTEMS 5.1 source already extracted."
fi

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

# ── Step 4: Apply Frama-C compatibility patches ───────────────────
echo "Applying Frama-C compatibility patches..."

# 4a. Comment out #include <limits.h> in scheduleredf.h
sed -i 's|^#include <limits\.h>|/* #include <limits.h> */ /* disabled for Frama-C */|' \
    "${RTEMS_SRC}/cpukit/include/rtems/score/scheduleredf.h"

# 4b. Comment out line 883 in thread.h
sed -i '883 s|^|// |' \
    "${RTEMS_SRC}/cpukit/include/rtems/score/thread.h"

# 4c. Replace flexible array _Per_CPU_Information[] with fixed [1U]
sed -i 's|Per_CPU_Control_envelope _Per_CPU_Information\[\]|Per_CPU_Control_envelope _Per_CPU_Information[1U]|' \
    "${RTEMS_SRC}/cpukit/include/rtems/score/percpu.h"

echo "Patches applied."

# ── Step 5: Copy verification files into RTEMS source tree ────────
echo "Copying verification files..."

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

echo "Verification files copied."

# ── Done ──────────────────────────────────────────────────────────
echo ""
echo "Setup complete. RTEMS source is at: rtems/src/rtems-5.1/"
echo ""
echo "Usage:"
echo "  docker compose run --rm verify verify-wp-all.sh -wp-model 'Typed+Cast'"
echo "  docker compose run --rm verify verify-edf-update-priority.sh -wp -wp-fct _Scheduler_EDF_Update_priority -wp-model 'Typed+Cast'"
echo "  xhost +local:docker && docker compose run --rm gui"
