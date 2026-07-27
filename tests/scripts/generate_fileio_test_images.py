#!/usr/bin/env python3
import pathlib


def write_bytes(path: pathlib.Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def main() -> None:
    repo = pathlib.Path(__file__).resolve().parents[2]
    image_dir = repo / "tests" / "images" / "generated"
    seq_dir = image_dir / "sequence"
    image_dir.mkdir(parents=True, exist_ok=True)
    seq_dir.mkdir(parents=True, exist_ok=True)

    width = 8
    height = 6

    # Mono GREY fixture: deterministic 0..47 ramp.
    grey = bytes([(x + y * width) & 0xFF for y in range(height) for x in range(width)])
    write_bytes(image_dir / "mono_8x6_GREY.raw", grey)

    # Bayer RGGB fixture with deterministic gradient.
    bayer = bytes([((x * 13) + (y * 17)) & 0xFF for y in range(height) for x in range(width)])
    write_bytes(image_dir / "bayer_8x6_RGGB.raw", bayer)

    # Two files for directory+wildcard tests.
    write_bytes(seq_dir / "frame_0001_8x6_GREY.raw", grey)
    seq2 = bytes([(v + 11) & 0xFF for v in grey])
    write_bytes(seq_dir / "frame_0002_8x6_GREY.raw", seq2)
    write_bytes(seq_dir / "ignore_8x6_GREY.raw", grey)


if __name__ == "__main__":
    main()
