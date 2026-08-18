FROM arm64v8/ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

# Ubuntu 20.04 pins the runtime to glibc 2.31/libstdc++ 3.4.28, the oldest
# baseline shared by JetPack 5.x (Xavier+) and Raspberry Pi OS bookworm.
# Node.js is intentionally not installed here: web UI assets are pre-built on
# the host (see build_runtime_arm64 in scripts/tasks.sh) before this image runs.
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    curl \
    ca-certificates \
    libeigen3-dev \
    libjpeg-dev \
    libpng-dev \
    libtiff-dev \
    libwebp-dev \
    python3 \
    python3-numpy \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
