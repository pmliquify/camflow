#!/usr/bin/env python3
import pathlib


def write_bytes(path: pathlib.Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def pack12(values: list[int]) -> bytes:
    packed = bytearray()
    for offset in range(0, len(values), 2):
        group = values[offset:offset + 2]
        packed.extend(value >> 4 for value in group)
        low_bits = 0
        for index, value in enumerate(group):
            low_bits |= (value & 0x0F) << (index * 4)
        packed.extend(low_bits.to_bytes((len(group) * 4 + 7) // 8, "little"))
    return bytes(packed)


def pack10(values: list[int]) -> bytes:
    packed = bytearray()
    for offset in range(0, len(values), 4):
        group = values[offset:offset + 4]
        packed.extend(value >> 2 for value in group)
        packed.append(sum((value & 0x03) << (index * 2) for index, value in enumerate(group)))
    return bytes(packed)


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

    # Red-only RGGB mosaics verify the Bayer pattern and packed 10-bit byte order.
    red_rggb = [255 if y % 2 == 0 and x % 2 == 0 else 0 for y in range(height) for x in range(width)]
    write_bytes(image_dir / "bayer_8x6_RGGB_red.raw", bytes(red_rggb))
    write_bytes(image_dir / "bayer_8x6_pRAA_red.raw", pack10([value * 4 + (3 if value else 0) for value in red_rggb]))

    # 12-bit packed Bayer fixture with an incomplete two-pixel group per row.
    packed_values = [0x012, 0x345, 0x678, 0x9AB, 0xCDE, 0xF01, 0x234, 0x567, 0x89A, 0xBCD, 0xEF0, 0x123]
    write_bytes(image_dir / "bayer_6x2_pRCC.raw", pack12(packed_values))
    write_bytes(image_dir / "bayer_6x2_pRCC_mono8.raw", bytes(value >> 4 for value in packed_values))

    # Two files for directory+wildcard tests.
    write_bytes(seq_dir / "frame_0001_8x6_GREY.raw", grey)
    seq2 = bytes([(v + 11) & 0xFF for v in grey])
    write_bytes(seq_dir / "frame_0002_8x6_GREY.raw", seq2)
    write_bytes(seq_dir / "ignore_8x6_GREY.raw", grey)


if __name__ == "__main__":
    main()
