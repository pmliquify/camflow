// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <string>

/**
 * @brief Enumeration of all pixel formats supported by the CamFlow pipeline.
 *
 * PixelFormat identifies the memory layout and colour model of a single image frame.
 * It is stored in every @ref ImageBuffer and used by @ref IImageConverter implementations
 * to select the appropriate conversion path.
 *
 * ### Naming convention
 * - `MonoN` – N-bit greyscale, 1 channel.
 * - `RGNP`, `GRNP`, etc. – raw Bayer patterns in 8, 10, 12 or 14 bit (unpacked unless
 *   the name ends in `P`, which indicates packed memory layout).
 * - `RGB888` / `BGR888` – 24-bit interleaved RGB / BGR.
 * - `YUYV` – 4:2:2 YUV packed.
 * - `NV12` – 4:2:0 YUV semi-planar.
 * - `Raw8/10/12/16` – driver-level raw formats without Bayer interpretation.
 */
enum class PixelFormat
{
    Unknown, ///< Format not recognised or not yet set.
    Mono8,   ///< 8-bit greyscale.
    Mono10,  ///< 10-bit greyscale (2 bytes per pixel, 6 MSB unused).
    Mono12,  ///< 12-bit greyscale (2 bytes per pixel, 4 MSB unused).
    Mono14,  ///< 14-bit greyscale (2 bytes per pixel, 2 MSB unused).
    Mono16,  ///< 16-bit greyscale.
    RG8,     ///< Bayer RGGB 8-bit.
    RG10,    ///< Bayer RGGB 10-bit (unpacked, 2 bytes per pixel).
    RG12,    ///< Bayer RGGB 12-bit (unpacked, 2 bytes per pixel).
    RG14,    ///< Bayer RGGB 14-bit (unpacked, 2 bytes per pixel).
    GR8,     ///< Bayer GRBG 8-bit.
    GR10,    ///< Bayer GRBG 10-bit (unpacked).
    GR12,    ///< Bayer GRBG 12-bit (unpacked).
    GR14,    ///< Bayer GRBG 14-bit (unpacked).
    BG8,     ///< Bayer BGGR 8-bit.
    BG10,    ///< Bayer BGGR 10-bit (unpacked).
    BG12,    ///< Bayer BGGR 12-bit (unpacked).
    BG14,    ///< Bayer BGGR 14-bit (unpacked).
    GB8,     ///< Bayer GBRG 8-bit.
    GB10,    ///< Bayer GBRG 10-bit (unpacked).
    GB12,    ///< Bayer GBRG 12-bit (unpacked).
    GB14,    ///< Bayer GBRG 14-bit (unpacked).
    RG10P,   ///< Bayer RGGB 10-bit packed.
    RG12P,   ///< Bayer RGGB 12-bit packed.
    RG14P,   ///< Bayer RGGB 14-bit packed.
    GR10P,   ///< Bayer GRBG 10-bit packed.
    GR12P,   ///< Bayer GRBG 12-bit packed.
    GR14P,   ///< Bayer GRBG 14-bit packed.
    BG10P,   ///< Bayer BGGR 10-bit packed.
    BG12P,   ///< Bayer BGGR 12-bit packed.
    BG14P,   ///< Bayer BGGR 14-bit packed.
    GB10P,   ///< Bayer GBRG 10-bit packed.
    GB12P,   ///< Bayer GBRG 12-bit packed.
    GB14P,   ///< Bayer GBRG 14-bit packed.
    RGB888,  ///< 24-bit RGB interleaved (R, G, B).
    BGR888,  ///< 24-bit BGR interleaved (B, G, R).
    YUYV,    ///< 4:2:2 YUV packed (Y0 U0 Y1 V0 ...).
    NV12,    ///< 4:2:0 YUV semi-planar (Y plane + interleaved UV plane).
    Raw8,    ///< Generic 8-bit raw (no Bayer interpretation).
    Raw10,   ///< Generic 10-bit raw.
    Raw12,   ///< Generic 12-bit raw.
    Raw16    ///< Generic 16-bit raw.
};

/** @brief Converts a @ref PixelFormat value to its canonical string name.
 *  @param format Format to convert.
 *  @return Lowercase string name (e.g. `"rg10"`, `"rgb888"`), or `"unknown"`. */
std::string pixelFormatToString(PixelFormat format);

/** @brief Converts a string name to the corresponding @ref PixelFormat value.
 *  @param value Case-insensitive format name.
 *  @return Corresponding @ref PixelFormat, or @ref PixelFormat::Unknown if not recognised. */
PixelFormat pixelFormatFromString(const std::string& value);

/** @brief Converts a @ref PixelFormat to its V4L2 / GStreamer fourcc code.
 *  @param format Format to convert.
 *  @return 32-bit fourcc code, or @c 0 if not mappable. */
uint32_t pixelFormatToFourCC(PixelFormat format);

/** @brief Converts a V4L2 fourcc code to a @ref PixelFormat.
 *  @param fourcc   32-bit fourcc code.
 *  @param fallback Value returned when @p fourcc is not recognised.
 *  @return Corresponding @ref PixelFormat or @p fallback. */
PixelFormat pixelFormatFromFourCC(uint32_t fourcc, PixelFormat fallback = PixelFormat::Unknown);

/** @brief Returns @c true if @p format is a raw Bayer mosaic format. */
bool isBayerFormat(PixelFormat format);

/** @brief Returns @c true if @p format uses packed (sub-byte-aligned) storage. */
bool isPackedFormat(PixelFormat format);

/** @brief Returns the number of bits per pixel for the given format.
 *  @param format Format to query.
 *  @return Bits per pixel (e.g. 8 for Mono8, 10 for RG10, 24 for RGB888). */
int bitsPerPixel(PixelFormat format);

/** @brief Returns the unpacked equivalent of a packed format.
 *
 *  For packed formats (e.g. @ref PixelFormat::RG10P) returns the corresponding
 *  unpacked format (e.g. @ref PixelFormat::RG10). For non-packed formats returns
 *  @p format unchanged.
 *
 *  @param format Format to unpack.
 *  @return Unpacked @ref PixelFormat. */
PixelFormat unpackedFormat(PixelFormat format);
