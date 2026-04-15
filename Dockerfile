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
# Stage 2: Final image with cross-compiler and Frama-C
# RTEMS source lives on the host and is mounted at runtime.
# =============================================================================
FROM ubuntu:22.04 AS final

ENV DEBIAN_FRONTEND=noninteractive
ENV HOME=/root

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

# Copy cross-compiler from builder
COPY --from=toolchain-builder /opt/rtems5/ /opt/rtems5/

ENV PATH=/opt/rtems5/bin:/opt/scripts:${PATH}

WORKDIR /workspace
CMD ["/bin/bash"]
