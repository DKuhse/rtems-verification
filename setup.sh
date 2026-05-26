#!/bin/bash
#
# Downloads RTEMS 5.1 + 6.2 pristine sources and builds the AMD64 BSP
# (via Docker) against the 5.1 pristine tree. The BSP build produces the
# generated headers (cpuopts.h, bspopts.h, ...) that the verify-*.sh
# scripts pull in via -I${RTEMS_BUILD_BSP}.
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
RTEMS_SRC_PRISTINE="${RTEMS_DIR}/src/rtems-5.1-pristine"
RTEMS_BUILD="${RTEMS_DIR}/build/amd64"
RTEMS_TARBALL="${RTEMS_DIR}/rtems-5.1.tar.xz"
RTEMS_URL="https://ftp.rtems.org/pub/rtems/releases/5/5.1/sources/rtems-5.1.tar.xz"
RTEMS62_SRC_PRISTINE="${RTEMS_DIR}/src/rtems-6.2-pristine"
RTEMS62_TARBALL="${RTEMS_DIR}/rtems-6.2.tar.xz"
RTEMS62_URL="https://ftp.rtems.org/pub/rtems/releases/6/6.2/sources/rtems-6.2.tar.xz"

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

# ── Step 2: Extract pristine sources ──────────────────────────────
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

# ── Step 3: Build AMD64 BSP via Docker (against pristine 5.1) ─────
# Produces generated headers under
# rtems/build/amd64/x86_64-rtems5/c/amd64/include that verify-*.sh
# scripts reference via -I${RTEMS_BUILD_BSP}.
if [ ! -f "${RTEMS_BUILD}/Makefile" ]; then
    echo "Building AMD64 BSP via Docker (this may take 10-20 minutes)..."
    docker compose run --rm toolchain bash -c '
        cd /workspace/rtems/build/amd64 &&
        /workspace/rtems/src/rtems-5.1-pristine/configure \
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

# ── Done ──────────────────────────────────────────────────────────
echo ""
echo "Setup complete."
echo "  Pristine 5.1 source: rtems/src/rtems-5.1-pristine/"
echo "  Pristine 6.2 source: rtems/src/rtems-6.2-pristine/"
echo "  AMD64 BSP build:     rtems/build/amd64/"
