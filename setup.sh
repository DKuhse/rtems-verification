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
RTEMS_SRC_PRISTINE="${RTEMS_DIR}/src/rtems-5.1-pristine"
RTEMS_BUILD="${RTEMS_DIR}/build/amd64"
RTEMS_TARBALL="${RTEMS_DIR}/rtems-5.1.tar.xz"
RTEMS_URL="https://ftp.rtems.org/pub/rtems/releases/5/5.1/sources/rtems-5.1.tar.xz"
RTEMS62_SRC="${RTEMS_DIR}/src/rtems-6.2"
RTEMS62_SRC_PRISTINE="${RTEMS_DIR}/src/rtems-6.2-pristine"
RTEMS62_TARBALL="${RTEMS_DIR}/rtems-6.2.tar.xz"
RTEMS62_URL="https://ftp.rtems.org/pub/rtems/releases/6/6.2/sources/rtems-6.2.tar.xz"
VERIFY_DIR="${SCRIPT_DIR}/Formally-Verifying-Implementations-of-EDF-Scheduler-in-RTEMS"
VERIFY62_DIR="${SCRIPT_DIR}/verification/6.2"

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

# ── Step 4b: Apply Frama-C compatibility patches (RTEMS 6.2) ──────
# These four patches make 6.2 headers parseable by Frama-C/WP. Each
# is idempotent: re-running the script re-applies cleanly.
echo "Applying Frama-C compatibility patches (6.2)..."

# 4b.1. Comment out #include <limits.h> in scheduleredf.h
sed -i 's|^#include <limits\.h>|/* #include <limits.h> */ /* disabled for Frama-C */|' \
    "${RTEMS62_SRC}/cpukit/include/rtems/score/scheduleredf.h"

# 4b.2 + 4b.4. Replace flexible array AND move CPU_STRUCTURE_ALIGNMENT
# attribute from type to variable position (so WP type matching works
# with our \separated clauses against _Per_CPU_Information).
sed -i 's|^extern CPU_STRUCTURE_ALIGNMENT Per_CPU_Control_envelope _Per_CPU_Information\[\];|extern Per_CPU_Control_envelope _Per_CPU_Information[1U] CPU_STRUCTURE_ALIGNMENT;|' \
    "${RTEMS62_SRC}/cpukit/include/rtems/score/percpu.h"

# 4b.3. Comment out flexible array members in thread.h. Both lines
# define RTEMS_ZERO_LENGTH_ARRAY members which cause WP to degenerate
# all \valid goals on these structs.
sed -i '/^  Thread_queue_Heads       Thread_queue_heads\[ RTEMS_ZERO_LENGTH_ARRAY \];/ s|^|// |' \
    "${RTEMS62_SRC}/cpukit/include/rtems/score/thread.h"
sed -i '/^  void                                 \*extensions\[ RTEMS_ZERO_LENGTH_ARRAY \];/ s|^|// |' \
    "${RTEMS62_SRC}/cpukit/include/rtems/score/thread.h"

echo "Patches (6.2) applied."

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

echo "Copying verification files (6.2)..."

# Stubs -> cpukit/
cp "${VERIFY62_DIR}/stubs.h" "${RTEMS62_SRC}/cpukit/"
cp "${VERIFY62_DIR}/release_cancel_stubs.h" "${RTEMS62_SRC}/cpukit/"

# Annotated headers -> cpukit/include/rtems/score/
cp "${VERIFY62_DIR}/priorityimpl.h" "${RTEMS62_SRC}/cpukit/include/rtems/score/"
cp "${VERIFY62_DIR}/scheduleredfimpl.h" "${RTEMS62_SRC}/cpukit/include/rtems/score/"
cp "${VERIFY62_DIR}/scheduleruniimpl.h" "${RTEMS62_SRC}/cpukit/include/rtems/score/"

# Annotated C sources -> cpukit/score/src/
cp "${VERIFY62_DIR}/scheduleredfchangepriority.c" "${RTEMS62_SRC}/cpukit/score/src/"
cp "${VERIFY62_DIR}/scheduleredfreleasejob.c" "${RTEMS62_SRC}/cpukit/score/src/"
cp "${VERIFY62_DIR}/scheduleredfunblock.c" "${RTEMS62_SRC}/cpukit/score/src/"
cp "${VERIFY62_DIR}/threadchangepriority.c" "${RTEMS62_SRC}/cpukit/score/src/"

echo "Verification files copied (6.2)."

# ── Done ──────────────────────────────────────────────────────────
echo ""
echo "Setup complete."
echo "  Modified (annotated) 5.1 source: rtems/src/rtems-5.1/"
echo "  Pristine            5.1 source: rtems/src/rtems-5.1-pristine/"
echo "  Modified (annotated) 6.2 source: rtems/src/rtems-6.2/"
echo "  Pristine            6.2 source: rtems/src/rtems-6.2-pristine/"
echo ""
echo "  Diff modified vs pristine (5.1): diff -ruN rtems/src/rtems-5.1-pristine/ rtems/src/rtems-5.1/"
echo "  Diff modified vs pristine (6.2): diff -ruN rtems/src/rtems-6.2-pristine/ rtems/src/rtems-6.2/"
echo ""
echo "Usage (RTEMS 5.1):"
echo "  docker compose run --rm verify verify-wp-all.sh -wp-model 'Typed+Cast'"
echo "  docker compose run --rm verify verify-edf-update-priority.sh -wp -wp-fct _Scheduler_EDF_Update_priority -wp-model 'Typed+Cast'"
echo ""
echo "Usage (RTEMS 6.2):"
echo "  docker compose run --rm verify /opt/scripts/6.2/verify-wp-all.sh -wp-model 'Typed+Cast' -wp-timeout 30"
echo "  docker compose run --rm verify /opt/scripts/6.2/verify-edf-update-priority.sh -wp -wp-fct _Scheduler_EDF_Update_priority -wp-model 'Typed+Cast'"
echo ""
echo "  xhost +local:docker && docker compose run --rm gui"
