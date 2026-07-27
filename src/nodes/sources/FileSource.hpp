// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "pipeline/Node.hpp"

#include <optional>
#include <string>
#include <vector>

/**
 * @brief Source node that reads image files from disk and injects them into the pipeline.
 *
 * FileSource reads one image file per frame from a configured file path or
 * directory. It supports encoded image files (JPEG, PNG, etc. via the system's
 * image library) as well as raw binary pixel data files.
 *
 * When a directory is configured all matching image files in that directory are
 * loaded in alphabetical order. When the last file is reached the source either
 * signals end-of-stream or loops back to the beginning depending on the `repeat`
 * parameter.
 *
 * ### Parameters
 * | Name        | Type   | Default | Description                                              |
 * |-------------|--------|---------|----------------------------------------------------------|
 * | `file`      | string | `""`    | Path to a single image file.                             |
 * | `directory` | string | `""`    | Path to a directory containing image files.              |
 * | `wildcard`  | string | `"*"`   | Optional filename wildcard for directory mode.            |
 * | `width`     | int    | `0`     | Width for raw files (pixels); ignored for encoded files. |
 * | `height`    | int    | `0`     | Height for raw files (pixels).                           |
 * | `stride`    | int    | `0`     | Row stride for raw files (bytes).                        |
 * | `bitShift`  | int    | `0`     | Right-shift metadata applied during RAW conversion/debayer. |
 * | `format`    | string | `""`    | Pixel format string for raw files (e.g. `"rg10"`).      |
 * | `repeat`    | bool   | `false` | Loop back to the first file after the last one.          |
 *
 * The `repeat` behavior is evaluated at runtime from scoped context key
 * `<thisNodeId>.repeat` for each processed frame.
 *
 * ### FrameContext output
 * Writes an `"image"` entry of type @ref ImageBuffer for each loaded frame.
 *
 * @see Node
 * @see ImageBuffer
 */
class FileSource : public Node
{
public:
    FileSource();

    /** @brief Returns `"filesrc"`. */
    std::string typeName() const override;

    /** @brief Returns a human-readable description of this source. */
    std::string description() const override;

    /** @brief Returns the parameter schema. */
    NodeSchema schema() const override;

    /**
     * @brief Discovers input files and validates configuration.
     * @return @c true if at least one input file was found.
     */
    bool init() override;

    /**
     * @brief Loads the next file and writes the image to @p context.
     *
     * Advances the file index each call. Returns @c false when all files
     * have been consumed (and `repeat` is @c false).
     *
     * @param context Per-frame data carrier; receives the `"image"` entry.
     * @return @c true while more frames are available; @c false at end-of-stream.
     */
    bool process(FrameContext& context) override;

private:
    /** @brief Scans the configured directory and collects matching file paths. */
    bool collectInputFiles();

    /** @brief Loads a single file (dispatches to encoded or raw loader). */
    bool loadFile(const std::string& fileName, FrameContext& context);

    /** @brief Loads an encoded image (JPEG, PNG, etc.) via the system image library. */
    bool loadEncodedImage(const std::string& fileName, FrameContext& context);

    /** @brief Loads a raw binary pixel file using configured geometry and format. */
    bool loadRawImage(const std::string& fileName, FrameContext& context);

    /** @brief Returns @c true if the file extension is a known encoded image format. */
    bool isEncodedImageFile(const std::string& fileName) const;

    /** @brief Returns @c true if the file extension is a supported image format. */
    bool isSupportedImageFile(const std::string& fileName) const;

    /** @brief Parses width/height from a filename fragment like `...1920x1080...`. */
    std::optional<std::pair<uint32_t, uint32_t>> parseDimensionsFromFileName(const std::string& fileName) const;

    /** @brief Maps a configured format string (including V4L2 names) to @ref PixelFormat. */
    PixelFormat parseFormatString(const std::string& value) const;

    /** @brief Tries to infer raw pixel format from file extension or name tokens. */
    PixelFormat inferRawFormatFromFileName(const std::string& fileName) const;

    std::vector<std::string> m_inputFiles; ///< Sorted list of discovered input files.
    size_t m_fileIndex;                    ///< Index of the next file to load.
    bool m_done;                           ///< @c true after end-of-stream has been signalled.
    uint64_t m_sequence;                   ///< Monotonically increasing output frame counter.
};
