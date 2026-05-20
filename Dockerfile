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
#
# FRAMA_C_VERSION selects the Frama-C/Why3/Alt-Ergo stack:
#   25.0 (default) -- legacy stack used for the published hand-port results
#                     (Frama-C 25 + Alt-Ergo 2.4.2 pinned).
#   32.0           -- active-port stack. Lets opam pick a compatible
#                     Alt-Ergo (no explicit pin). FC 25's wp.driver has a
#                     vset packaging bug that breaks set-typed `assigns`
#                     clauses needed for the EDF model.
# =============================================================================
FROM ubuntu:22.04 AS final

ARG FRAMA_C_VERSION=25.0
ARG INSTALL_COQ=false

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
# For FRAMA_C_VERSION=25.0 we pin Alt-Ergo to 2.4.2 (matches the published
# legacy results). For newer Frama-C, let opam resolve a compatible Alt-Ergo.
RUN opam init --disable-sandboxing --compiler=4.14.1 -y \
    && eval $(opam env) \
    && if [ "${FRAMA_C_VERSION}" = "25.0" ]; then \
         opam pin add alt-ergo 2.4.2 -y --no-action \
         && opam install frama-c.${FRAMA_C_VERSION} alt-ergo.2.4.2 -y; \
       else \
         opam install frama-c.${FRAMA_C_VERSION} alt-ergo -y; \
       fi \
    && eval $(opam env) \
    && why3 config detect

RUN echo 'eval $(opam env)' >> ${HOME}/.bashrc

# Copy cross-compiler from builder
COPY --from=toolchain-builder /opt/rtems5/ /opt/rtems5/

ENV PATH=/opt/rtems5/bin:/opt/scripts:${PATH}

# Optional interactive prover support for WP.  Why3 1.8.2's Coq package
# supports the Coq 8.x line directly; Coq/Rocq 9.x is not detected cleanly by
# this Frama-C/Why3 stack.
RUN if [ "${INSTALL_COQ}" = "true" ]; then \
      eval $(opam env) \
      && opam install coq.8.16.1 why3-coq.1.8.2 -y \
      && why3 config detect; \
    fi

WORKDIR /workspace
CMD ["/bin/bash"]
