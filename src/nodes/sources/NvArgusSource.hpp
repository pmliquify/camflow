// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "pipeline/Node.hpp"

#include <cstdint>
#include <string>

/// Forward declaration of GStreamer pipeline element (opaque pointer to avoid GStreamer include).
typedef struct _GstElement GstElement;

/**
 * @brief Source node that captures video from NVIDIA Jetson camera sensors via GStreamer and nvarguscamerasrc.
 *
 * NvArgusSource uses GStreamer and the NVIDIA Argus camera framework on Jetson platforms
 * to capture from CSI cameras. It builds a GStreamer pipeline, manages auto-exposure and
 * white-balance locks, and delivers frames as @ref ImageBuffer outputs.
 *
 * This node is only available on NVIDIA Jetson platforms with GStreamer and Argus libraries
 * installed. It is conditionally compiled (see `CMakeLists.txt` for `ARGUS_FOUND`).
 *
 * ### Parameters
 * | Name              | Type | Default | Description                              |
 * |-------------------|------|---------|------------------------------------------|
 * | `sensorId`        | int  | `0`     | Camera sensor index.                     |
 * | `aeLock`          | bool | `false` | Lock auto-exposure.                      |
 * | `aeLeft`          | int  | `0`     | AE ROI left coordinate.                  |
 * | `aeTop`           | int  | `0`     | AE ROI top coordinate.                   |
 * | `aeWidth`         | int  | `0`     | AE ROI width (0 = full frame).           |
 * | `aeHeight`        | int  | `0`     | AE ROI height (0 = full frame).          |
 * | `gainRange`       | int  | `0`     | Analog gain range.                       |
 * | `ispDigitalGainRange` | int | `0` | ISP digital gain range.               |
 * | `awbLock`         | bool | `false` | Lock auto-white-balance.                 |
 * | `wbMode`          | int  | `0`     | White-balance mode.                      |
 * | `tnrMode`         | int  | `0`     | Temporal noise reduction mode.           |
 * | `width`           | int  | `1920`  | Output frame width.                      |
 * | `height`          | int  | `1080`  | Output frame height.                     |
 * | `frameRate`       | int  | `30`    | Target frame rate in frames per second.  |
 *
 * Values are published into scoped context keys (for example
 * `<thisNodeId>.sensorId`) before each frame is processed.
 *
 * ### GStreamer pipeline
 * The node constructs a pipeline of the form:
 * @code
 * nvarguscamerasrc sensor-id=N ! videorate ! videoconvert ! ...
 * @endcode
 * with various properties set based on the configuration parameters.
 *
 * @see Node
 * @see ImageBuffer
 */
class NvArgusSource : public Node
{
public:
    NvArgusSource();
    ~NvArgusSource() override;

    /** @brief Returns `"nvargussrc"`. */
    std::string typeName() const override;

    /** @brief Returns a human-readable description. */
    std::string description() const override;

    /** @brief Returns the parameter schema with all Argus-specific parameters. */
    NodeSchema schema() const override;

    /**
     * @brief Builds and starts the GStreamer pipeline.
     * @return @c true if the pipeline was created and started successfully.
     */
    bool init() override;

    /**
     * @brief Retrieves the next frame from the GStreamer pipeline.
     *
     * Waits for a buffer to be available in the sink pad and wraps it as an
     * @ref ImageBuffer zero-copy reference.
     *
     * @param context Per-frame data carrier; receives the `"image"` entry.
     * @return @c true on success; @c false if a timeout or error occurs.
     */
    bool process(FrameContext& context) override;

    /** @brief Stops and destroys the GStreamer pipeline. */
    void shutdown() override;

private:
    /**
     * @brief Constructs the complete GStreamer pipeline description.
     * @return Pipeline description string (e.g. `"nvarguscamerasrc ... ! ..."`)
     */
    std::string pipelineDescription() const;

    // --- GStreamer state ---
    GstElement* m_pipeline; ///< GStreamer pipeline element (nullptr if not initialised).
    GstElement* m_sink;     ///< Reference to the app sink element for frame retrieval.

    uint64_t m_sequence; ///< Monotonically increasing frame counter.
};
