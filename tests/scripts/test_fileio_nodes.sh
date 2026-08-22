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

node_available() {
    "${BIN}" --help | awk -v node="$1" '$1 == node { found=1 } END { exit found ? 0 : 1 }'
}

# 1) RAW passthrough with explicit width/height/format.
run_pipeline "filesrc(file=tests/images/generated/mono_8x6_GREY.raw,width=8,height=6,format=GREY)->filesink(format=raw,filename=tests/output/raw_passthrough,appendDatetime=false,appendSequence=false,appendPixelFormat=false,appendImageSize=false)" 1
cmp -s "${IMG_DIR}/mono_8x6_GREY.raw" "${ROOT_DIR}/tests/output/raw_passthrough.raw"

# 2) Filename dimension parsing (...8x6...) without width/height params.
run_pipeline "filesrc(file=tests/images/generated/mono_8x6_GREY.raw,format=GREY)->filesink(format=raw,filename=tests/output/raw_filename_dimensions,appendDatetime=false,appendSequence=false,appendPixelFormat=false,appendImageSize=false)" 1
cmp -s "${IMG_DIR}/mono_8x6_GREY.raw" "${ROOT_DIR}/tests/output/raw_filename_dimensions.raw"

# 3) Explicit width/height override filename dimensions.
run_pipeline "filesrc(file=tests/images/generated/mono_8x6_GREY.raw,width=4,height=4,format=GREY)->filesink(format=raw,filename=tests/output/raw_override_dimensions,appendDatetime=false,appendSequence=false,appendPixelFormat=false,appendImageSize=false)" 1
[[ $(wc -c < "${ROOT_DIR}/tests/output/raw_override_dimensions.raw") -eq 16 ]]

# 3a) Default append parameters are all enabled and follow <name>_<datetime>_<seq>_<format>_<width>x<height>.<suffix>.
run_pipeline "filesrc(file=tests/images/generated/mono_8x6_GREY.raw,width=8,height=6,format=GREY)->filesink(filename=tests/output/defaults,format=raw)" 1
[[ $(find "${ROOT_DIR}/tests/output" -maxdepth 1 -name 'defaults_????????_??????_*_GREY_8x6.raw' | wc -l) -eq 1 ]]

# 3aa) A missing directory prefix in filename is created automatically.
run_pipeline "filesrc(file=tests/images/generated/mono_8x6_GREY.raw,width=8,height=6,format=GREY)->filesink(format=raw,filename=tests/output/new_nested/dir/frame,appendDatetime=false,appendSequence=false,appendPixelFormat=false,appendImageSize=false)" 1
[[ -s "${ROOT_DIR}/tests/output/new_nested/dir/frame.raw" ]]

# 3b) bitShift metadata is applied before RAW conversion/debayer path.
run_pipeline "filesrc(file=tests/images/generated/bayer_8x6_RGGB.raw,format=RGGB,width=8,height=6,bitShift=1)->debayer->filesink(format=png,filename=tests/output/debayer_shifted,appendDatetime=false,appendSequence=false,appendPixelFormat=false,appendImageSize=false)" 1
[[ -s "${ROOT_DIR}/tests/output/debayer_shifted.png" ]]

# 4) Directory + wildcard + repeat=false should read only matching files.
run_pipeline "filesrc(directory=tests/images/generated/sequence,wildcard=frame_*_GREY.raw,format=GREY,repeat=false)->filesink(format=raw,filename=tests/output/directory_stream,appendDatetime=false,appendSequence=true,appendPixelFormat=false,appendImageSize=false)" 3
[[ $(find "${ROOT_DIR}/tests/output" -maxdepth 1 -name 'directory_stream_*.raw' | wc -l) -eq 2 ]]

# 5) repeat=true should create potentially endless stream (bounded by -n).
run_pipeline "filesrc(file=tests/images/generated/mono_8x6_GREY.raw,format=GREY,width=8,height=6,repeat=true)->filesink(format=raw,filename=tests/output/repeat_stream,appendDatetime=false,appendSequence=true,appendPixelFormat=false,appendImageSize=false)" 3
[[ $(find "${ROOT_DIR}/tests/output" -maxdepth 1 -name 'repeat_stream_*.raw' | wc -l) -eq 3 ]]

# 5b) appendDatetime uses write time formatted as YYYYMMDD_hhmmss.
run_pipeline "filesrc(file=tests/images/generated/mono_8x6_GREY.raw,format=GREY,width=8,height=6)->filesink(format=raw,filename=tests/output/timestamped,appendDatetime=true,appendSequence=false,appendPixelFormat=false,appendImageSize=false)" 1
[[ $(find "${ROOT_DIR}/tests/output" -maxdepth 1 -name 'timestamped_????????_??????.raw' | wc -l) -eq 1 ]]

# 6) RAW Bayer to PNG/JPG through FileSink is converted to greyscale (no automatic demosaicing).
run_pipeline "filesrc(file=tests/images/generated/bayer_8x6_RGGB.raw,format=RGGB,width=8,height=6)->filesink(format=png,filename=tests/output/debayer,appendDatetime=false,appendSequence=false,appendPixelFormat=false,appendImageSize=false)" 1
[[ -s "${ROOT_DIR}/tests/output/debayer.png" ]]

run_pipeline "filesrc(file=tests/images/generated/bayer_8x6_RGGB.raw,format=RGGB,width=8,height=6)->filesink(format=jpg,filename=tests/output/debayer,appendDatetime=false,appendSequence=false,appendPixelFormat=false,appendImageSize=false)" 1
[[ -s "${ROOT_DIR}/tests/output/debayer.jpg" ]]

# 6b) Implicit RAW->BGR conversion of Bayer input yields R=G=B (greyscale), unlike the debayer node.
run_pipeline "filesrc(file=tests/images/generated/bayer_8x6_RGGB_red.raw,format=RGGB,width=8,height=6)->filesink(format=png,filename=tests/output/no_auto_debayer_src,appendDatetime=false,appendSequence=false,appendPixelFormat=false,appendImageSize=false)" 1
run_pipeline "filesrc(file=tests/output/no_auto_debayer_src.png)->filesink(format=raw,filename=tests/output/no_auto_debayer,appendDatetime=false,appendSequence=false,appendPixelFormat=false,appendImageSize=false)" 1
python3 - "${ROOT_DIR}/tests/output/no_auto_debayer.raw" <<'PY'
import pathlib
import sys

pixel = pathlib.Path(sys.argv[1]).read_bytes()[(2 * 8 + 2) * 3:(2 * 8 + 3) * 3]
if pixel[0] != pixel[1] or pixel[1] != pixel[2]:
    raise SystemExit(f"expected greyscale (R=G=B) without an explicit debayer node, got {list(pixel)}")
PY

# 7) PNG/JPG source without width/height/stride, written back as a RAW 1:1 dump.
run_pipeline "filesrc(file=tests/output/debayer.png)->filesink(format=raw,filename=tests/output/from_png,appendDatetime=false,appendSequence=false,appendPixelFormat=false,appendImageSize=false)" 1
[[ -s "${ROOT_DIR}/tests/output/from_png.raw" ]]

run_pipeline "filesrc(file=tests/output/debayer.jpg)->filesink(format=raw,filename=tests/output/from_jpg,appendDatetime=false,appendSequence=false,appendPixelFormat=false,appendImageSize=false)" 1
[[ -s "${ROOT_DIR}/tests/output/from_jpg.raw" ]]

# 8) Debayer processor writes BGR888 as a RAW 1:1 dump (8x6x3 bytes).
run_pipeline "filesrc(file=tests/images/generated/bayer_8x6_RGGB.raw,format=RGGB,width=8,height=6)->debayer->filesink(format=raw,filename=tests/output/debayer_processor,appendDatetime=false,appendSequence=false,appendPixelFormat=false,appendImageSize=false)" 1
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

run_pipeline "filesrc(file=tests/images/generated/bayer_8x6_RGGB_red.raw,format=RGGB,width=8,height=6)->debayer->filesink(format=raw,filename=tests/output/debayer_rggb_red,appendDatetime=false,appendSequence=false,appendPixelFormat=false,appendImageSize=false)" 1
assert_red_debayer "${ROOT_DIR}/tests/output/debayer_rggb_red.raw"

run_pipeline "filesrc(file=tests/images/generated/bayer_8x6_pRAA_red.raw,format=pRAA,width=8,height=6)->debayer->filesink(format=raw,filename=tests/output/debayer_rg10p_red,appendDatetime=false,appendSequence=false,appendPixelFormat=false,appendImageSize=false)" 1
assert_red_debayer "${ROOT_DIR}/tests/output/debayer_rg10p_red.raw"

# 9) CCM processor (RAW input is converted to greyscale, not demosaiced).
if node_available ccm; then
    run_pipeline "filesrc(file=tests/images/generated/bayer_8x6_RGGB.raw,format=RGGB,width=8,height=6)->ccm(m00=1.05,m11=1.0,m22=0.95)->filesink(format=png,filename=tests/output/ccm,appendDatetime=false,appendSequence=false,appendPixelFormat=false,appendImageSize=false)" 1
    [[ -s "${ROOT_DIR}/tests/output/ccm.png" ]]
fi

echo "FileSource/FileSink/Debayer/CCM tests passed."
