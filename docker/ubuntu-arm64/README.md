# CamFlow ARM64 Docker Build Environment

This image provides a native ARM64 Ubuntu build environment for CamFlow.
It provides the dependencies required to build the runtime with OpenCV.
It also installs Node.js and npm because the built-in React UI is bundled as
part of the normal CMake runtime build.
It is intended to be used with Docker/QEMU or on an ARM64 host. The source
folder is mounted into `/workspace`, so build output is written back to the
same project directory.

Build the image:

```bash
docker build --platform linux/arm64 -t camflow-arm64 docker/ubuntu-arm64
```

Build CamFlow inside the container:

```bash
docker run --rm --platform linux/arm64 \
    -v "$(pwd):/workspace" \
    -w /workspace \
    camflow-arm64 \
    bash -lc 'cmake -B build-runtime-arm64 -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-runtime-arm64 --parallel'
```
