#!/usr/bin/env bash
set -euo pipefail

OPENCV_VERSION="4.12.0"
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
PROJECT_DIR=$(cd -- "${SCRIPT_DIR}/../.." && pwd)

HOST_ARCH=$(uname -m)
if [ "${HOST_ARCH}" = "amd64" ]; then
    HOST_ARCH="x86_64"
fi
if [ "${HOST_ARCH}" = "aarch64" ]; then
    HOST_ARCH="arm64"
fi

INSTALL_PREFIX="${1:-${PROJECT_DIR}/opencv/${OPENCV_VERSION}/linux-${HOST_ARCH}}"
BUILD_ROOT="${2:-${PROJECT_DIR}/opencv/${OPENCV_VERSION}/build-linux-${HOST_ARCH}}"
BUILD_PROFILE="release-fast-v1-${HOST_ARCH}"
BUILD_PROFILE_STAMP="${INSTALL_PREFIX}/.camflow-build-profile"
OPENCV_ARCH_ARGS=()

case "${HOST_ARCH}" in
    x86_64)
        OPENCV_ARCH_ARGS+=(
            -DWITH_IPP=ON
            -DCPU_BASELINE=SSE3
            -DCPU_DISPATCH=SSE4_1,SSE4_2,FP16,AVX,AVX2,AVX512_SKX
        )
        ;;
    arm64)
        OPENCV_ARCH_ARGS+=(
            -DWITH_IPP=OFF
            -DCPU_BASELINE=NEON
            -DCPU_DISPATCH=
        )
        ;;
    *)
        OPENCV_ARCH_ARGS+=(-DWITH_IPP=OFF)
        ;;
esac

normalize_opencv_exports() {
    local static_libdir="/usr/lib/$(uname -m)-linux-gnu"
    if [ -d "/usr/lib/x86_64-linux-gnu" ] && [ "$(uname -m)" = "x86_64" ]; then
        static_libdir="/usr/lib/x86_64-linux-gnu"
    elif [ -d "/usr/lib/aarch64-linux-gnu" ] && [ "$(uname -m)" = "aarch64" ]; then
        static_libdir="/usr/lib/aarch64-linux-gnu"
    fi

    local modules_file
    for modules_file in \
        "${INSTALL_PREFIX}/lib/cmake/opencv4/OpenCVModules.cmake" \
        "${INSTALL_PREFIX}/lib/cmake/opencv4/OpenCVModules-release.cmake"; do
        if [ -f "${modules_file}" ]; then
            sed -i "s#${static_libdir}/libjpeg.so.8#${static_libdir}/libjpeg.a#g" "${modules_file}"
            sed -i "s#${static_libdir}/libjpeg.so#${static_libdir}/libjpeg.a#g" "${modules_file}"
            sed -i "s#${static_libdir}/libpng16.so.16#${static_libdir}/libpng.a#g" "${modules_file}"
            sed -i "s#${static_libdir}/libpng.so#${static_libdir}/libpng.a#g" "${modules_file}"
        fi
    done
}

if [ -f "${INSTALL_PREFIX}/lib/cmake/opencv4/OpenCVConfig.cmake" ] &&
    [ -f "${BUILD_PROFILE_STAMP}" ] &&
    [ "$(cat "${BUILD_PROFILE_STAMP}")" = "${BUILD_PROFILE}" ]; then
    normalize_opencv_exports
    echo "OpenCV ${OPENCV_VERSION} (${BUILD_PROFILE}) already installed in ${INSTALL_PREFIX}"
    echo "OpenCV_DIR=${INSTALL_PREFIX}/lib/cmake/opencv4" >> "${GITHUB_ENV:-/dev/null}" || true
    return 0 2>/dev/null || exit 0
fi

if [ "${CAMFLOW_SKIP_APT:-0}" != "1" ]; then
    APT_PREFIX=""
    if command -v sudo >/dev/null 2>&1; then
        APT_PREFIX="sudo"
    fi
    ${APT_PREFIX} apt-get update
    ${APT_PREFIX} apt-get install -y \
        build-essential \
        cmake \
        git \
        ninja-build \
        pkg-config \
        libeigen3-dev \
        libjpeg-dev \
        libpng-dev \
        zlib1g-dev
fi

rm -rf "${BUILD_ROOT}"
mkdir -p "${BUILD_ROOT}"
cd "${BUILD_ROOT}"

git clone --depth 1 --branch "${OPENCV_VERSION}" https://github.com/opencv/opencv.git opencv

cmake -S opencv -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_LIST=core,imgproc,imgcodecs \
    -DBUILD_TESTS=OFF \
    -DBUILD_PERF_TESTS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_opencv_apps=OFF \
    -DBUILD_DOCS=OFF \
    -DBUILD_PROTOBUF=ON \
    -DWITH_PROTOBUF=ON \
    -DBUILD_ADE=ON \
    -DWITH_1394=OFF \
    -DWITH_FFMPEG=OFF \
    -DWITH_GSTREAMER=OFF \
    -DWITH_GTK=OFF \
    -DWITH_QT=OFF \
    -DWITH_OPENEXR=OFF \
    -DWITH_TIFF=OFF \
    -DWITH_WEBP=OFF \
    -DWITH_OPENCL=OFF \
    -DCV_ENABLE_INTRINSICS=ON \
    -DENABLE_FAST_MATH=ON \
    -DENABLE_LTO=ON \
    "${OPENCV_ARCH_ARGS[@]}"

cmake --build build --parallel
cmake --install build

# OpenCV 4.12 may export optional imported targets (ade/protobuf) in OpenCVModules.cmake
# even when those archives are not installed for the selected BUILD_LIST.
# Create tiny fallback archives so dependent CMake config checks remain consistent.
mkdir -p "${INSTALL_PREFIX}/lib/opencv4/3rdparty"
if [ ! -f "${INSTALL_PREFIX}/lib/opencv4/3rdparty/libade.a" ] || [ ! -f "${INSTALL_PREFIX}/lib/opencv4/3rdparty/liblibprotobuf.a" ]; then
    cat > "${BUILD_ROOT}/camflow_opencv_stub.c" <<'EOF'
void camflow_opencv_stub(void) {}
EOF
    cc -c "${BUILD_ROOT}/camflow_opencv_stub.c" -o "${BUILD_ROOT}/camflow_opencv_stub.o"
fi
if [ ! -f "${INSTALL_PREFIX}/lib/opencv4/3rdparty/libade.a" ]; then
    ar rcs "${INSTALL_PREFIX}/lib/opencv4/3rdparty/libade.a" "${BUILD_ROOT}/camflow_opencv_stub.o"
fi
if [ ! -f "${INSTALL_PREFIX}/lib/opencv4/3rdparty/liblibprotobuf.a" ]; then
    ar rcs "${INSTALL_PREFIX}/lib/opencv4/3rdparty/liblibprotobuf.a" "${BUILD_ROOT}/camflow_opencv_stub.o"
fi

normalize_opencv_exports
printf '%s\n' "${BUILD_PROFILE}" > "${BUILD_PROFILE_STAMP}"

echo "OpenCV_DIR=${INSTALL_PREFIX}/lib/cmake/opencv4" >> "${GITHUB_ENV:-/dev/null}" || true