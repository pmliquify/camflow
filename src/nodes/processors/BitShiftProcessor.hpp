// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "pipeline/Node.hpp"

/**
 * @brief Processor node that either applies a bit shift conversion or publishes shift metadata.
 *
 * BitShiftProcessor accepts a raw or Bayer image on the `image` input.
 *
 * - If `apply=true`, it applies the configured right shift and converts the image to
 *   @ref PixelFormat::BGR888.
 * - If `apply=false`, it forwards the image unchanged and only publishes the configured
 *   `bitshift` value into the @ref FrameContext.
 *
 * @see ImageConverter
 * @see ImageBuffer
 */
class BitShiftProcessor : public Node
{
public:
    std::string typeName() const override;
    std::string description() const override;
    NodeSchema schema() const override;
    bool process(FrameContext& context) override;
};
