#!/usr/bin/env bash
set -euo pipefail

# Build a minimal, LGPLv2.1+, zero-external-dependency static FFmpeg containing
# ONLY the Bink (.bik) demuxer, Bink video/audio decoders, and libswscale.
#
# Supports 64-bit Linux (aarch64 / x86_64) and macOS (arm64 / x86_64).
# Output: <repo>/third_party/ffmpeg-bink/{include,lib/lib*.a}
# When present, CMake links these archives statically into `openhazard-sp` so
# Bink cutscenes work out of the box without system FFmpeg dependencies.

FFMPEG_VERSION="${FFMPEG_VERSION:-7.1.1}"
FFMPEG_URL="https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
PREFIX="${1:-${REPO_ROOT}/third_party/ffmpeg-bink}"

WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/openhazard-ffmpeg-bink.XXXXXX")"
trap 'rm -rf "${WORK_DIR}"' EXIT

OS_NAME="$(uname -s)"
ARCH_NAME="$(uname -m)"

if command -v nproc >/dev/null 2>&1; then
  JOBS="$(nproc)"
elif [[ "${OS_NAME}" == "Darwin" ]]; then
  JOBS="$(sysctl -n hw.ncpu)"
else
  JOBS=4
fi

EXTRA_CFLAGS="-O2 -fPIC"
EXTRA_CFG=()
if [[ "${OS_NAME}" == "Linux" ]]; then
  if [[ "${ARCH_NAME}" == "aarch64" || "${ARCH_NAME}" == "arm64" ]]; then
    # Universal ARMv8.0-A baseline (Pi 3/4/5, RK3588, all 64-bit ARM SBCs)
    EXTRA_CFLAGS="${EXTRA_CFLAGS} -march=armv8-a -mtune=cortex-a76"
  elif [[ "${ARCH_NAME}" == "x86_64" ]]; then
    EXTRA_CFLAGS="${EXTRA_CFLAGS} -march=x86-64"
    EXTRA_CFG+=(--disable-x86asm)
  fi
elif [[ "${OS_NAME}" == "Darwin" ]]; then
  EXTRA_CFLAGS="${EXTRA_CFLAGS} -mmacosx-version-min=12.0"
  if [[ "${ARCH_NAME}" == "x86_64" ]]; then
    EXTRA_CFG+=(--disable-x86asm)
  fi
fi

echo "==> Downloading FFmpeg ${FFMPEG_VERSION} (LGPLv2.1+ Bink-only build, ${OS_NAME}/${ARCH_NAME})..."
curl -fSL "${FFMPEG_URL}" -o "${WORK_DIR}/ffmpeg.tar.xz"

echo "==> Extracting..."
tar -xJf "${WORK_DIR}/ffmpeg.tar.xz" -C "${WORK_DIR}"
SRC_DIR="${WORK_DIR}/ffmpeg-${FFMPEG_VERSION}"

STAGE_DIR="${WORK_DIR}/stage"
mkdir -p "${WORK_DIR}/build" "${STAGE_DIR}" "${PREFIX}"
cd "${WORK_DIR}/build"

echo "==> Configuring minimal Bink-only static FFmpeg -> ${PREFIX}"
"${SRC_DIR}/configure" \
  --prefix="${STAGE_DIR}" \
  --enable-static \
  --disable-shared \
  --enable-pic \
  --disable-programs \
  --disable-doc \
  --disable-debug \
  --disable-network \
  --disable-autodetect \
  --disable-avdevice \
  --disable-avfilter \
  --disable-postproc \
  --disable-swresample \
  --disable-everything \
  --enable-demuxer=bink \
  --enable-decoder=bink,binkaudio_dct,binkaudio_rdft \
  --enable-swscale \
  "${EXTRA_CFG[@]}" \
  --extra-cflags="${EXTRA_CFLAGS}"

echo "==> Compiling on ${JOBS} cores..."
make -j"${JOBS}"
make install

rm -rf "${PREFIX}/include" "${PREFIX}/lib"
cp -a "${STAGE_DIR}/include" "${STAGE_DIR}/lib" "${PREFIX}/"
rm -rf "${PREFIX}/lib/pkgconfig"

echo "==> Done! Static Bink FFmpeg installed to ${PREFIX}:"
ls -lh "${PREFIX}/lib"/lib*.a
