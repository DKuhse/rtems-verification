# =============================================================================
# Stage 1: Build RTEMS 5.1 x86_64 cross-toolchain via RSB
# This is the heaviest stage (~1-2 hours). Once cached, it never rebuilds.
# =============================================================================
FROM ubuntu:22.04 AS toolchain-builder

ENV DEBIAN_FRONTEND=noninteractive
ENV RTEMS_PREFIX=/opt/rtems5

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential gcc g++ make \
    bison flex texinfo \
    unzip pax \
    python3 python3-dev \
    libncurses5-dev zlib1g-dev \
    curl ca-certificates xz-utils \
    git \
    && rm -rf /var/lib/apt/lists/*

# Download and extract RTEMS Source Builder 5.1
WORKDIR /opt/rsb-src
RUN curl -fSL https://ftp.rtems.org/pub/rtems/releases/5/5.1/sources/rtems-source-builder-5.1.tar.xz \
    | tar xJ --strip-components=1

# Remove GDB and rtems-tools from the build set — we only need the
# cross-compiler (binutils + gcc + newlib) for Frama-C preprocessing.
RUN sed -i '/rtems-gdb/d; /rtems-tools/d' rtems/config/5/rtems-x86_64.bset

# Build x86_64 cross-toolchain (GCC 9.3.0, binutils, newlib — no GDB)
WORKDIR /opt/rsb-src/rtems
RUN ../source-builder/sb-set-builder --prefix=${RTEMS_PREFIX} 5/rtems-x86_64


# =============================================================================
# Stage 2: Build RTEMS 5.1 AMD64 BSP
# Produces the generated config headers needed by Frama-C preprocessing.
# =============================================================================
FROM toolchain-builder AS bsp-builder

ENV PATH=/opt/rtems5/bin:${PATH}

# Download and extract RTEMS 5.1 kernel source
WORKDIR /opt/rtems-src
RUN curl -fSL https://ftp.rtems.org/pub/rtems/releases/5/5.1/sources/rtems-5.1.tar.xz \
    | tar xJ

# Configure and build the AMD64 BSP
RUN mkdir -p /opt/rtems-build/amd64
WORKDIR /opt/rtems-build/amd64
RUN /opt/rtems-src/rtems-5.1/configure \
        --prefix=/opt/rtems5 \
        --enable-maintainer-mode \
        --target=x86_64-rtems5 \
        --enable-rtemsbsp=amd64 \
    && make \
    && make install


# =============================================================================
# Stage 3: Final image with Frama-C, RTEMS toolchain, and verification files
# =============================================================================
FROM ubuntu:22.04 AS final

ENV DEBIAN_FRONTEND=noninteractive
ENV HOME=/root
ENV RTEMS_HOME=${HOME}/RTEMS
ENV RTEMS_PREFIX=${RTEMS_HOME}/rtems_x86_64
ENV RTEMS_SRC=${RTEMS_HOME}/src/rtems-5.1
ENV RTEMS_BUILD=${RTEMS_HOME}/build

# Runtime dependencies, opam, and optional X11 libs for frama-c-gui
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential m4 pkg-config \
    opam \
    autoconf automake \
    libgmp-dev \
    libgtk-3-dev libgtksourceview-3.0-dev \
    graphviz \
    curl ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install Frama-C BEFORE adding RTEMS toolchain to PATH, so opam uses
# system autoconf/automake (RTEMS ships its own which confuses opam builds).
RUN opam init --disable-sandboxing --compiler=4.14.1 -y \
    && eval $(opam env) \
    && opam pin add alt-ergo 2.4.2 -y --no-action \
    && opam install frama-c.25.0 alt-ergo.2.4.2 -y \
    && eval $(opam env) \
    && why3 config detect

RUN echo 'eval $(opam env)' >> ${HOME}/.bashrc

# Copy toolchain + installed BSP from builder
COPY --from=bsp-builder /opt/rtems5/ ${RTEMS_PREFIX}/

# Copy BSP build output (generated config headers)
COPY --from=bsp-builder /opt/rtems-build/amd64/ ${RTEMS_BUILD}/amd64/

# Copy RTEMS source tree (needed for cpukit headers)
COPY --from=bsp-builder /opt/rtems-src/rtems-5.1/ ${RTEMS_SRC}/

ENV PATH=${RTEMS_PREFIX}/bin:/opt/scripts:${PATH}

# ---------------------------------------------------------------------------
# Apply 3 source patches for Frama-C compatibility (README section:
# "Source Code Modification to make it run with Frama-C")
# ---------------------------------------------------------------------------

# 1. Comment out #include <limits.h> in scheduleredf.h (line 29)
RUN sed -i 's|^#include <limits\.h>|/* #include <limits.h> */ /* disabled for Frama-C */|' \
    ${RTEMS_SRC}/cpukit/include/rtems/score/scheduleredf.h

# 2. Comment out line 883 in thread.h
RUN sed -i '883 s|^|// |' \
    ${RTEMS_SRC}/cpukit/include/rtems/score/thread.h

# 3. Replace flexible array _Per_CPU_Information[] with fixed [1U] in percpu.h (line 620)
RUN sed -i 's|Per_CPU_Control_envelope _Per_CPU_Information\[\]|Per_CPU_Control_envelope _Per_CPU_Information[1U]|' \
    ${RTEMS_SRC}/cpukit/include/rtems/score/percpu.h

# ---------------------------------------------------------------------------
# Copy annotated verification files into the RTEMS source tree
# ---------------------------------------------------------------------------

# Stub files -> cpukit/
COPY Formally-Verifying-Implementations-of-EDF-Scheduler-in-RTEMS/stubs.h \
     Formally-Verifying-Implementations-of-EDF-Scheduler-in-RTEMS/release_cancel_stubs.h \
     ${RTEMS_SRC}/cpukit/

# Annotated headers -> cpukit/include/rtems/score/
COPY Formally-Verifying-Implementations-of-EDF-Scheduler-in-RTEMS/priorityimpl.h \
     Formally-Verifying-Implementations-of-EDF-Scheduler-in-RTEMS/scheduleredfimpl.h \
     Formally-Verifying-Implementations-of-EDF-Scheduler-in-RTEMS/schedulerimpl.h \
     ${RTEMS_SRC}/cpukit/include/rtems/score/

# Annotated C sources -> cpukit/score/src/
COPY Formally-Verifying-Implementations-of-EDF-Scheduler-in-RTEMS/scheduleredfchangepriority.c \
     Formally-Verifying-Implementations-of-EDF-Scheduler-in-RTEMS/scheduleredfreleasejob.c \
     Formally-Verifying-Implementations-of-EDF-Scheduler-in-RTEMS/scheduleredfunblock.c \
     Formally-Verifying-Implementations-of-EDF-Scheduler-in-RTEMS/threadchangepriority.c \
     ${RTEMS_SRC}/cpukit/score/src/

# ---------------------------------------------------------------------------
# Verification convenience scripts
# ---------------------------------------------------------------------------
COPY scripts/ /opt/scripts/
RUN chmod +x /opt/scripts/*.sh

WORKDIR ${RTEMS_SRC}/cpukit
CMD ["/bin/bash"]
