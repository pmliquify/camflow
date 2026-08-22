// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "pipeline/Node.hpp"

/**
 * @brief Processor node that applies a 3x3 Color Correction Matrix (CCM).
 *
 * CCMProcessor transforms each BGR pixel by a configurable 3x3 matrix. For
 * non-BGR formats it requests a conversion to BGR888 through the injected
 * image converter. Bayer RAW input is not automatically demosaiced; it is
 * converted to greyscale like any other RAW format. Use a `debayer` node
 * upstream if color output from Bayer RAW input is required.
 *
 * ### Parameters
 * | Name | Type   | Default | Description |
 * |------|--------|---------|-------------|
 * | m00..m22 | double | identity matrix values | Scoped context dependencies read from `<thisNodeId>.m00` ... `<thisNodeId>.m22` |
 *
 * @see ImageConverter
 * @see IImageConverter
 * @see FrameContext
 */
class CCMProcessor : public Node
{
public:
    /** @brief Returns `ccm` as graph/pipeline node type. */
    std::string typeName() const override;

    /** @brief Returns a human-readable description of this processor. */
    std::string description() const override;

    /** @brief Returns parameter schema for 3x3 CCM coefficients. */
    NodeSchema schema() const override;

    /**
     * @brief Converts to BGR888 if needed (RAW input becomes greyscale, not demosaiced) and then applies the CCM.
     * @param context Frame context containing key `image` with @ref ImageBuffer.
     * @return @c true on success, @c false if conversion or processing fails.
     */
    bool process(FrameContext& context) override;
};
