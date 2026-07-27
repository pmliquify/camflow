// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "pipeline/Node.hpp"

#include <cstdint>

/**
 * @brief Non-destructive pipeline inspection node that logs per-frame diagnostics.
 *
 * Probe is a special-purpose @ref Node with @ref NodeKind::Probe role. It sits
 * anywhere in the pipeline and logs diagnostic information for every frame it
 * processes without modifying the @ref FrameContext. The frame data is passed
 * through unchanged so that downstream nodes receive the same data as if the
 * probe were not present.
 *
 * ### Logged information
 * For every frame the probe logs:
 * - The probe identifier from @ref Node::id().
 * - The list of all keys present in the @ref FrameContext.
 * - The image geometry and pixel format if an `"image"` entry is present.
 * - The inter-frame interval and estimated frame rate derived from the image
 *   timestamp, if available.
 *
 * ### Pipeline DSL
 * Probes are inserted in CLI expressions using the `-id>` link syntax:
 * @code
 * v4l2src -CAM0> tcpsink
 * @endcode
 * This inserts a probe named `"CAM0"` between the source and the sink.
 *
 * @see Node
 * @see CLIPipelineParser
 */
class Probe : public Node
{
public:
    /**
     * @brief Returns the type name `"probe"` used for factory registration.
     * @return `"probe"`
     */
    std::string typeName() const override;

    /**
     * @brief Returns a description of the Probe node's purpose.
     * @return Human-readable description string.
     */
    std::string description() const override;

    /**
     * @brief Returns the parameter schema.
     *
     * Probe does not define any configurable parameters.
     *
     * @return Empty @ref ParameterSchema.
     */
    NodeSchema schema() const override;

    /**
     * @brief Logs frame diagnostics without modifying @p context.
     *
     * Uses the node id as probe label, logs per-frame information (context keys,
     * image geometry, frame rate) and forwards the frame unchanged when an image
     * is present. If the required image entry is missing the method returns
     * @c false so the pipeline can stop cleanly.
     *
     * @param context  Per-frame data carrier; read-only access only.
     * @return @c true when an image was processed successfully; @c false if the
     *         required image entry is missing.
     */
    bool process(FrameContext& context) override;

private:
    bool m_hasPreviousTimestamp = false; ///< @c true after the first frame has been processed.
    uint64_t m_previousTimestampNs = 0;  ///< Timestamp of the previous frame for inter-frame calculation.
};
