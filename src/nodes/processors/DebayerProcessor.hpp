// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "pipeline/Node.hpp"

/**
 * @brief Processor node that debayers RAW Bayer images to BGR888.
 *
 * DebayerProcessor accepts exactly one image input from FrameContext key
 * `image`. The input must be one of the Bayer RAW formats (8/10/12/14 bit,
 * packed or unpacked). The node converts the input to @ref PixelFormat::BGR888
 * using the injected image converter and writes the converted image back to key
 * `image`.
 *
 * This node intentionally rejects non-Bayer input formats to make graph
 * behavior explicit and fail-fast.
 *
 * @see ImageConverter
 * @see IImageConverter
 * @see FrameContext
 */
class DebayerProcessor : public Node
{
public:
    /** @brief Returns `debayer` as graph/pipeline node type. */
    std::string typeName() const override;

    /** @brief Returns a human-readable description of this processor. */
    std::string description() const override;

    /** @brief Returns an empty schema (no parameters). */
    NodeSchema schema() const override;

    /**
     * @brief Debayers the input image.
     * @param context Frame context containing key `image` with @ref ImageBuffer.
     * @return @c true on success, @c false on unsupported input or conversion failure.
     */
    bool process(FrameContext& context) override;
};
