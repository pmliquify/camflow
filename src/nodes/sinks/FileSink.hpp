// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "pipeline/Node.hpp"

#include "image/PixelFormat.hpp"

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
 * | Name              | Type   | Description                                                  |
 * |-------------------|--------|--------------------------------------------------------------|
 * | `file`            | string | Base output file path (e.g. `/tmp/frame.raw`).               |
 * | `format`          | string | Optional target RAW format for non-encoded outputs.          |
 * | `appendSequence`  | bool   | If @c true, appends the sequence number to the file name.    |
 * | `appendTimestamp` | bool   | If @c true, appends the timestamp (ns) to the file name.     |
 *
 * During processing these values are read from scoped context keys
 * `<thisNodeId>.file`, `<thisNodeId>.format`, `<thisNodeId>.appendSequence`
 * and `<thisNodeId>.appendTimestamp`.
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

    /** @brief Returns the parameter schema with `file`, `appendSequence` and `appendTimestamp`. */
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
    /** @brief Returns @c true for encoded image targets (`.png`, `.jpg`, `.jpeg`). */
    bool isEncodedTarget(const std::string& fileName) const;

    /** @brief Converts configured target format strings (including V4L2 names). */
    PixelFormat parseFormatString(const std::string& value) const;

    /**
     * @brief Constructs the output file name for the given frame.
     * @param sequence    Frame sequence number.
     * @param timestampNs Frame capture timestamp in nanoseconds.
     * @return Full output file path string.
     */
    std::string outputFileName(const std::string& baseFileName, bool appendSequence, bool appendTimestamp, uint64_t sequence, uint64_t timestampNs) const;
};
