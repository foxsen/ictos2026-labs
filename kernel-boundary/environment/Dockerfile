FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    cpio \
    device-tree-compiler \
    git \
    libarchive-tools \
    ninja-build \
    pkg-config \
    protobuf-compiler \
    python3 \
    python3-pip \
    python3-venv \
    qemu-system-x86 \
    repo \
    xz-utils

RUN python3 -m venv /opt/ictos-venv && \
    /opt/ictos-venv/bin/pip install --no-cache-dir sel4-deps==0.7.0

ENV PATH=/opt/ictos-venv/bin:${PATH}
WORKDIR /lab
CMD ["/bin/bash"]
