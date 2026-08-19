#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
BIN="${1:-${ROOT_DIR}/build-runtime/src/runtime/camflow}"
OUT_DIR="${ROOT_DIR}/tests/output"
IMG_DIR="${ROOT_DIR}/tests/images/generated"

if [[ ! -x "${BIN}" ]]; then
    echo "Runtime executable not found: ${BIN}" >&2
    exit 1
fi

python3 "${ROOT_DIR}/tests/scripts/generate_fileio_test_images.py"

rm -rf "${OUT_DIR}"
mkdir -p "${OUT_DIR}"

run_pipeline() {
    local frames="$2"
    local expression="$1"
    "${BIN}" -n "${frames}" "${expression}"
}

# 1) RAW passthrough with explicit width/height/format.
run_pipeline "filesrc(file=tests/images/generated/mono_8x6_GREY.raw,width=8,height=6,format=GREY)->filesink(file=tests/output/raw_passthrough.raw)" 1
cmp -s "${IMG_DIR}/mono_8x6_GREY.raw" "${ROOT_DIR}/tests/output/raw_passthrough.raw"

# 2) Filename dimension parsing (...8x6...) without width/height params.
run_pipeline "filesrc(file=tests/images/generated/mono_8x6_GREY.raw,format=GREY)->filesink(file=tests/output/raw_filename_dimensions.raw)" 1
cmp -s "${IMG_DIR}/mono_8x6_GREY.raw" "${ROOT_DIR}/tests/output/raw_filename_dimensions.raw"

# 3) Explicit width/height override filename dimensions.
run_pipeline "filesrc(file=tests/images/generated/mono_8x6_GREY.raw,width=4,height=4,format=GREY)->filesink(file=tests/output/raw_override_dimensions.raw)" 1
[[ $(wc -c < "${ROOT_DIR}/tests/output/raw_override_dimensions.raw") -eq 16 ]]

# 3b) bitShift metadata is applied before RAW conversion/debayer path.
run_pipeline "filesrc(file=tests/images/generated/bayer_8x6_RGGB.raw,format=RGGB,width=8,height=6,bitShift=1)->debayer->filesink(file=tests/output/debayer_shifted.png)" 1
[[ -s "${ROOT_DIR}/tests/output/debayer_shifted.png" ]]

# 3c) Packed 12-bit Bayer converts losslessly to the expected display byte.
run_pipeline "filesrc(file=tests/images/generated/bayer_6x2_pRCC.raw,format=pRCC,width=6,height=2)->filesink(file=tests/output/packed12_mono8.raw,format=GREY)" 1
cmp -s "${IMG_DIR}/bayer_6x2_pRCC_mono8.raw" "${ROOT_DIR}/tests/output/packed12_mono8.raw"

# 4) Directory + wildcard + repeat=false should read only matching files.
run_pipeline "filesrc(directory=tests/images/generated/sequence,wildcard=frame_*_GREY.raw,format=GREY,repeat=false)->filesink(file=tests/output/directory_stream.raw,appendSequence=true)" 3
[[ $(find "${ROOT_DIR}/tests/output" -maxdepth 1 -name 'directory_stream_seq*.raw' | wc -l) -eq 2 ]]

# 5) repeat=true should create potentially endless stream (bounded by -n).
run_pipeline "filesrc(file=tests/images/generated/mono_8x6_GREY.raw,format=GREY,width=8,height=6,repeat=true)->filesink(file=tests/output/repeat_stream.raw,appendSequence=true)" 3
[[ $(find "${ROOT_DIR}/tests/output" -maxdepth 1 -name 'repeat_stream_seq*.raw' | wc -l) -eq 3 ]]

# 6) RAW Bayer to PNG/JPG through FileSink auto-debayer.
run_pipeline "filesrc(file=tests/images/generated/bayer_8x6_RGGB.raw,format=RGGB,width=8,height=6)->filesink(file=tests/output/debayer.png)" 1
[[ -s "${ROOT_DIR}/tests/output/debayer.png" ]]

run_pipeline "filesrc(file=tests/images/generated/bayer_8x6_RGGB.raw,format=RGGB,width=8,height=6)->filesink(file=tests/output/debayer.jpg)" 1
[[ -s "${ROOT_DIR}/tests/output/debayer.jpg" ]]

# 7) PNG/JPG source without width/height/stride.
run_pipeline "filesrc(file=tests/output/debayer.png)->filesink(file=tests/output/from_png.raw,format=BGR3)" 1
[[ -s "${ROOT_DIR}/tests/output/from_png.raw" ]]

run_pipeline "filesrc(file=tests/output/debayer.jpg)->filesink(file=tests/output/from_jpg.raw,format=BGR3)" 1
[[ -s "${ROOT_DIR}/tests/output/from_jpg.raw" ]]

# 8) Debayer processor.
run_pipeline "filesrc(file=tests/images/generated/bayer_8x6_RGGB.raw,format=RGGB,width=8,height=6)->debayer->filesink(file=tests/output/debayer_processor.raw,format=BGR3)" 1
[[ $(wc -c < "${ROOT_DIR}/tests/output/debayer_processor.raw") -eq 144 ]]

# 8b) RGGB and packed RG10P must interpolate red sensels into BGR red, not cyan.
assert_red_debayer() {
    python3 - "$1" <<'PY'
import pathlib
import sys

pixel = pathlib.Path(sys.argv[1]).read_bytes()[(2 * 8 + 2) * 3:(2 * 8 + 3) * 3]
if pixel != bytes((0, 0, 255)):
    raise SystemExit(f"expected BGR red at RGGB center, got {list(pixel)}")
PY
}

run_pipeline "filesrc(file=tests/images/generated/bayer_8x6_RGGB_red.raw,format=RGGB,width=8,height=6)->debayer->filesink(file=tests/output/debayer_rggb_red.raw,format=BGR3)" 1
assert_red_debayer "${ROOT_DIR}/tests/output/debayer_rggb_red.raw"

run_pipeline "filesrc(file=tests/images/generated/bayer_8x6_pRAA_red.raw,format=pRAA,width=8,height=6)->debayer->filesink(file=tests/output/debayer_rg10p_red.raw,format=BGR3)" 1
assert_red_debayer "${ROOT_DIR}/tests/output/debayer_rg10p_red.raw"

# 9) CCM processor (auto debayer when RAW input).
run_pipeline "filesrc(file=tests/images/generated/bayer_8x6_RGGB.raw,format=RGGB,width=8,height=6)->ccm(m00=1.05,m11=1.0,m22=0.95)->filesink(file=tests/output/ccm.png)" 1
[[ -s "${ROOT_DIR}/tests/output/ccm.png" ]]

# 10) Save BGR image as YUYV and reload it.
run_pipeline "filesrc(file=tests/output/debayer.png)->filesink(file=tests/output/converted_8x6_YUYV.raw,format=YUYV)" 1
[[ $(wc -c < "${ROOT_DIR}/tests/output/converted_8x6_YUYV.raw") -eq 96 ]]

run_pipeline "filesrc(file=tests/output/converted_8x6_YUYV.raw,width=8,height=6,format=YUYV)->filesink(file=tests/output/from_yuyv.png)" 1
[[ -s "${ROOT_DIR}/tests/output/from_yuyv.png" ]]

# 11) Save BGR image as NV12 and reload it.
run_pipeline "filesrc(file=tests/output/debayer.png)->filesink(file=tests/output/converted_8x6_NV12.raw,format=NV12)" 1
[[ $(wc -c < "${ROOT_DIR}/tests/output/converted_8x6_NV12.raw") -eq 72 ]]

run_pipeline "filesrc(file=tests/output/converted_8x6_NV12.raw,width=8,height=6,format=NV12)->filesink(file=tests/output/from_nv12.jpg)" 1
[[ -s "${ROOT_DIR}/tests/output/from_nv12.jpg" ]]

echo "FileSource/FileSink/Debayer/CCM tests passed."
