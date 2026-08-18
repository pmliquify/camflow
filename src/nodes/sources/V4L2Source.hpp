// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "image/ImageBuffer.hpp"
#include "pipeline/Node.hpp"
#include "V4L2Control.hpp"
#include "V4L2Device.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

/**
 * @brief Source node that captures video frames from V4L2 (Video4Linux2) camera devices.
 *
 * V4L2Source opens a V4L2 device (typically `/dev/videoN`) and optional sub-devices,
 * sets up MMAP-based buffer streaming, and produces an @ref ImageBuffer for every
 * frame captured. It uses zero-copy @ref ImageBuffer::wrapExternal to present kernel
 * buffers directly downstream; the kernel buffer is queued back only when all pipeline
 * references are released.
 *
 * ### Lifecycle
 * 1. **configure**: Applies generic internal settings from @ref Node and
 *    refreshes control metadata.
 * 2. **init**: Opens devices, sets format, allocates MMAP buffers, enumerates controls.
 * 3. **start**: Marks the capture path as active.
 * 4. **process**: Waits for the next buffer, wraps it zero-copy and returns it.
 * 5. **onParameterChanged**: Reopens changed devices or updates camera controls.
 * 6. **stop**: Stops streaming before shutdown.
 * 7. **shutdown**: Deallocates buffers and closes devices.
 *
 * ### Parameters
 * | Name          | Type   | Startup behavior                                     |
 * |---------------|--------|------------------------------------------------------|
 * | `device`      | string | Selected device path (default `/dev/video0`).        |
 * | `subdevices`  | option | Optional multi-select list of sub-device paths.      |
 * | `pixelformat` | option | Uses device start value unless explicitly configured. |
 * | `width`       | int    | Uses device start value unless explicitly configured. |
 * | `height`      | int    | Uses device start value unless explicitly configured. |
 * | `bitShift`    | int    | Metadata shift applied before image conversion.       |
 * | controls...   | mixed  | Discovered dynamically from V4L2 control metadata.   |

 * V4L2 controls from the device and all selected subdevices are enumerated dynamically
 * and exposed as runtime parameters when the device is available.
 *
 * Parameters that are not explicitly set in the node configuration are treated as
 * device-derived startup values. Explicit values are tracked and re-applied across
 * reopen/restart operations.
 *
 * ### Zero-copy streaming
 * The kernel MMAP buffer pointer and its requeue callback are wrapped in an
 * @ref ImageBuffer via @ref ImageBuffer::wrapExternal. The kernel buffer is
 * automatically queued back when the ImageBuffer is destroyed or a new frame
 * is captured (the shared_ptr owner's deleter is called).
 *
 * ### Thread model
 * @ref process is called from the pipeline thread and blocks (with timeout) until
 * a frame is available. Long-running capture should be on the main thread;
 * do not call from high-priority real-time threads as ioctl may block.
 *
 * @see Node
 * @see V4L2Control
 * @see ImageBuffer
 */
class V4L2Source : public Node
{
public:
    V4L2Source();
    ~V4L2Source() override;

    /** @brief Returns `"v4l2src"`. */
    std::string typeName() const override;

    /** @brief Returns a human-readable description. */
    std::string description() const override;

    /**
     * @brief Returns the parameter schema.
     *
     * Declares device, subdevices, format, width, height, buffers and timeout
     * parameters, plus all V4L2 camera controls discovered by @ref refreshControlSchema.
     *
     * @return ParameterSchema for all available parameters.
     */
    NodeSchema schema() const override;

    /**
     * @brief Applies configuration parameters and refreshes discovered controls.
     *
     * Explicitly provided parameter names are tracked so startup and reopen logic can
     * distinguish configured overrides from device-derived defaults.
     *
     * @param parameters Node configuration values.
     * @return @c true if configuration succeeded.
     */
    bool configure(const ParameterSet& parameters) override;

    /**
     * @brief Opens devices, sets format, allocates MMAP buffers and discovers controls.
     * @return @c true if all operations succeeded.
     */
    bool init() override;

    /** @brief Marks the source as started after capture initialization. */
    bool start() override;

    /**
     * @brief Dequeues the next captured frame and wraps it zero-copy.
     *
     * Blocks until a frame is available (with timeout) and writes the @ref ImageBuffer
     * as an `"image"` entry to @p context.
     *
     * @param context Per-frame data carrier; receives the `"image"` entry.
     * @return @c true on success; @c false if dequeue times out or fails.
     */
    bool process(FrameContext& context) override;

    /** @brief Stops streaming, deallocates buffers and closes devices. */
    void shutdown() override;

    /** @brief Stops streaming before shutdown. */
    void stop() override;

    /** @brief Returns a snapshot of currently configured internal runtime parameters. */
    ParameterSet currentParameters() const override;

protected:
    /**
     * @brief Reopens changed devices or applies writable controls directly.
     */
    bool onParameterChanged(const std::string& name, const ParameterValue& value, const ParameterValue* previousValue, std::string& errorMessage) override;

private:
    // --- Device management ---
    /** @brief Opens the device and all selected sub-device file descriptors. */
    bool openDevice();
    /** @brief Closes the device and all sub-device file descriptors. */
    void closeDevice();
    /** @brief Reopens sub-device file descriptors from the current `subdevices` parameter. */
    void syncOpenSubDevicesLocked();

    // --- Streaming management ---
    bool startCapture();
    void stopCapture();

    // --- Control management ---
    /** @brief Refreshes the list of available camera controls from the device. */
    bool refreshControlSchema();
    /** @brief Applies all configured control values to the device hardware. */
    bool applyConfiguredControls();
    /** @brief Applies one configured control while m_mutex is already held. */
    bool applyControlParameterLocked(const std::string& name, const ParameterValue& value, std::string* errorMessage = nullptr);
    void refreshDeviceOptions();
    void refreshFormatOptions() const;
    bool isExplicitParameter(const std::string& name) const;
    bool applyRequestedFormat();
    void refreshCurrentParameterValues() const;
    /** @brief Selects the appropriate V4L2 fourcc code for the configured format. */
    uint32_t selectedFourcc() const;
    /** @brief Updates m_width, m_height, m_stride from the active device format. */
    void updateImageGeometry();

    // --- Test mode ---
    /** @brief Generates a synthetic test frame when no device is connected. */
    bool generateTestFrame(FrameContext& context);

    // --- Active output metadata ---
    mutable PixelFormat m_pixelFormat;                       ///< Active output pixel format.
    mutable uint32_t m_width;                                ///< Configured and actual frame width.
    mutable uint32_t m_height;                               ///< Configured and actual frame height.
    mutable uint32_t m_stride;                               ///< Actual row stride from device format.
    uint32_t m_bufferCount;                                  ///< Internal MMAP buffer count.
    uint32_t m_timeoutUs;                                    ///< Internal dequeue timeout.
    mutable ParameterSchema m_controlSchema;                 ///< Discovered V4L2 controls.
    std::vector<V4L2Control> m_controls;                     ///< All discovered camera controls.
    std::map<std::string, V4L2Control> m_controlByParameter; ///< Lookup by CamFlow parameter name.
    std::vector<std::string> m_deviceOptions;
    std::vector<std::string> m_deviceOptionLabels;
    std::vector<std::string> m_subDeviceOptions;
    std::vector<std::string> m_subDeviceOptionLabels;
    mutable std::vector<std::string> m_formatOptions;
    mutable std::vector<std::string> m_formatOptionLabels;
    uint64_t m_sequence; ///< Monotonically increasing frame counter.

    // --- Device state ---
    V4L2Device m_device;                                   ///< V4L2 video device wrapper.
    std::vector<std::unique_ptr<V4L2Device>> m_subDevices; ///< V4L2 sub-device wrappers selected by the `subdevices` parameter.
    mutable std::mutex m_mutex;
    std::unordered_set<std::string> m_explicitParameters;
};
