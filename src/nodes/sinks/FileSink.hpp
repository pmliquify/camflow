// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "pipeline/Node.hpp"

/**
 * @brief Sink node that writes frames to image files on disk.
 *
 * FileSink consumes @ref ImageBuffer frames from the @ref FrameContext and
 * writes them either as raw binary data or as encoded image files depending on
 * the configured output target. When encoded output is requested, the node uses
 * the injected image converter to normalize frames before writing them. The
 * output file name can optionally include the frame sequence number and/or
 * capture timestamp to produce unique per-frame file names.
 *
 * ### Parameters
 * | Name               | Type   | Description                                                       |
 * |--------------------|--------|--------------------------------------------------------------------|
 * | `filename`         | string | Base output file path without file extension.                     |
 * | `appendDatetime`   | bool   | If @c true, appends the write date/time (`YYYYMMDD_hhmmss`).       |
 * | `appendSequence`   | bool   | If @c true, appends the frame sequence number.                     |
 * | `appendPixelFormat`| bool   | If @c true, appends the written image's pixel format (fourcc-like).|
 * | `appendImageSize`  | bool   | If @c true, appends the image size as `<width>x<height>`.          |
 * | `appendBitShift`   | bool   | If @c true, appends the image's bit shift as `bs<N>` (omitted when 0).|
 * | `format`           | option | Output file format (`jpg`, `png`, `raw`), used as file extension.  |
 *
 * The resulting file name is assembled as
 * `<filename>_<datetime>_<sequence>_<pixelFormat>_<width>x<height>_bs<N>.<format>`,
 * omitting any component whose `append*` parameter is disabled (and `bs<N>` whenever
 * the bit shift is 0). RAW output writes the in-memory image buffer byte-for-byte (no
 * pixel format conversion), so file size always matches `width * height * bytesPerPixel`;
 * preserving the bit shift in the file name lets @ref FileSource restore it on reload.
 *
 * During processing these values are read from scoped context keys
 * `<thisNodeId>.filename`, `<thisNodeId>.appendDatetime`, `<thisNodeId>.appendSequence`,
 * `<thisNodeId>.appendPixelFormat`, `<thisNodeId>.appendImageSize`, `<thisNodeId>.appendBitShift`
 * and `<thisNodeId>.format`.
 *
 * @see Node
 * @see ImageBuffer
 */
class FileSink : public Node
{
public:
    FileSink() = default;

    /** @brief Returns `"filesink"`. */
    std::string typeName() const override;

    /** @brief Returns a human-readable description of this sink. */
    std::string description() const override;

    /** @brief Returns the parameter schema with `filename`, the `append*` toggles and `format`. */
    NodeSchema schema() const override;

    /**
     * @brief Writes the current frame's image buffer to the configured file.
     *
     * Reads the `"image"` entry from @p context. If no image is present the
     * frame is silently skipped. Runtime sink options are read from scoped
     * context keys and the file name is constructed from the active values.
     *
     * @param context Per-frame data carrier containing the image to write.
     * @return @c true on success; @c false if the file could not be written.
     */
    bool process(FrameContext& context) override;

private:
    /**
     * @brief Constructs the output file name for the given frame.
     * @param baseFileName     Configured `filename` value (without extension).
     * @param extension        File extension including the leading dot.
     * @param appendDatetime   Whether to append the write date/time.
     * @param appendSequence   Whether to append the frame sequence number.
     * @param appendPixelFormat Whether to append the written pixel format.
     * @param appendImageSize  Whether to append `<width>x<height>`.
     * @param appendBitShift   Whether to append `bs<N>` (only when the bit shift is nonzero).
     * @param sequence         Frame sequence number.
     * @param pixelFormatName  Written image's pixel format name.
     * @param width            Written image width in pixels.
     * @param height           Written image height in pixels.
     * @param bitShift         Written image's bit shift.
     * @return Full output file path string.
     */
    std::string outputFileName(const std::string& baseFileName, const std::string& extension, bool appendDatetime, bool appendSequence, bool appendPixelFormat, bool appendImageSize,
                               bool appendBitShift, uint64_t sequence, const std::string& pixelFormatName, uint32_t width, uint32_t height, uint8_t bitShift) const;
};
