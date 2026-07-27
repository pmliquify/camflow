// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "pipeline/Node.hpp"

/**
 * @brief Composites multiple scoped images into a single output frame.
 *
 * CompositorProcessor combines several scoped input images into one output
 * frame. It is designed for graphs where separate branches produce images in
 * parallel and a later stage needs to place them together using per-scope
 * coordinates and layer ordering.
 *
 * The processor expects the node configuration to provide explicit input scopes
 * such as `compositor{cam0,cam1}`. Each listed scope is treated as one layer
 * source, and the processor reads the image and optional placement metadata from
 * that scope.
 *
 * ### FrameContext keys
 * For every configured input scope, the key `<scope>.image` must be present.
 * The output image is written back as logical key `image`, which the scheduler
 * publishes into the compositor's own write scope as `<thisNodeId>.image`.
 *
 * ### Parameters
 * CompositorProcessor has no node-local parameters. Instead, it reads optional
 * scoped context parameters per input layer:
 * `xpos`, `ypos`, `zorder`.
 *
 * Parameters are resolved from scoped FrameContext keys like `cam0.xpos`,
 * including dynamic keys written by upstream nodes or by scoped CLI assignments.
 *
 * @see Node
 * @see ImageBuffer
 * @see IImageConverter
 * @see FrameContext
 */
class CompositorProcessor : public Node
{
public:
    /** @brief Returns `"compositor"`. */
    std::string typeName() const override;

    /** @brief Returns a human-readable description. */
    std::string description() const override;

    /** @brief Returns the compositor-style parameter schema. */
    NodeSchema schema() const override;

    /**
     * @brief Composites all available input images and writes the result.
     *
     * Reads all images from the configured input scopes in @p context,
     * applies scoped `xpos`, `ypos` and `zorder` context dependencies,
     * and writes the combined output as `"image"`.
     *
     * @param context Per-frame data carrier with input image entries.
     * @return @c true on success; @c false if input images are missing or incompatible.
     */
    bool process(FrameContext& context) override;
};