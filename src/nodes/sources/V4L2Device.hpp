// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <linux/videodev2.h>
#include <string>
#include <vector>

/**
 * @brief Thin RAII wrapper around a Linux V4L2 device file descriptor.
 *
 * V4L2Device centralizes all low-level ioctl, poll and mmap interactions needed
 * by @ref V4L2Source. It can either own a file descriptor opened from a device
 * path or attach to an already-open descriptor without taking ownership.
 *
 * Responsibilities:
 * - opening and closing video or sub-device nodes
 * - querying and applying active image formats
 * - requesting, mapping, queueing and dequeueing capture buffers
 * - starting and stopping streaming capture
 * - exposing the active capture format and buffer geometry to higher layers
 *
 * The class deliberately stays close to the V4L2 kernel API. It does not hide
 * the native `v4l2_*` structures; instead it provides a safer lifetime boundary
 * around them so higher-level code can share one implementation for device
 * management while keeping actual V4L2 semantics visible.
 *
 * @see V4L2Source
 * @see V4L2ControlAccess
 */
class V4L2Device
{
public:
    /**
     * @brief One dequeued capture result returned from @ref captureFrame.
     *
     * The payload pointer references memory owned by the active MMAP buffer set.
     * It remains valid until the corresponding buffer is re-queued or capture is
     * stopped.
     */
    struct CaptureFrame
    {
        uint8_t* data = nullptr;
        size_t size = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t stride = 0;
        uint32_t fourcc = 0;
        uint64_t sequence = 0;
        uint64_t timestampNs = 0;
    };

    /**
     * @brief One pixel format entry reported by the device.
     *
     * Instances of this type are returned by @ref enumerateSupportedFormats and
     * expose both the raw FourCC and human-readable names for UI/parameter use.
     */
    struct SupportedFormat
    {
        uint32_t fourcc = 0;
        std::string fourccName;
        std::string description;
    };

    /**
     * @brief Bookkeeping for one mapped V4L2 streaming buffer.
     *
     * Stores the kernel `v4l2_buffer`, per-plane descriptors, mapped addresses
     * and mapped lengths so the full buffer set can be released reliably.
     */
    struct MappedBuffer
    {
        v4l2_buffer buffer{};
        std::vector<v4l2_plane> planes;
        std::vector<uint8_t*> ptrs;
        std::vector<size_t> sizes;
    };

    /** @brief Creates a closed device wrapper with no attached descriptor. */
    V4L2Device();

    /** @brief Stops streaming if needed, unmaps buffers and closes the descriptor. */
    ~V4L2Device();

    V4L2Device(const V4L2Device&) = delete;
    V4L2Device& operator=(const V4L2Device&) = delete;

    /** @brief Opens a V4L2 device node and takes ownership of the resulting descriptor. */
    bool open(const std::string& path, int flags);

    /** @brief Attaches to an existing descriptor without taking ownership. */
    bool attachBorrowed(int fd, const std::string& name);

    /** @brief Stops streaming, releases buffers and closes or detaches the descriptor. */
    void close();

    /** @brief Returns @c true if a valid file descriptor is currently attached. */
    bool isOpen() const;

    /** @brief Returns the active file descriptor, or @c -1 if closed. */
    int fd() const;

    /** @brief Returns the human-readable device name or path associated with this wrapper. */
    const std::string& name() const;

    /** @brief Queries device capabilities via `VIDIOC_QUERYCAP`. */
    bool queryCapability(v4l2_capability& capability) const;

    /** @brief Enumerates all stream formats currently advertised by the device. */
    std::vector<SupportedFormat> enumerateSupportedFormats() const;

    /** @brief Returns @c true if the device advertises streaming capture support. */
    bool supportsStreamingCapture() const;

    /** @brief Reads the active V4L2 format into @p format. */
    bool getFormat(v4l2_format& format) const;

    /** @brief Applies a V4L2 format request and refreshes the cached active format. */
    bool setFormat(v4l2_format& format) const;

    /** @brief Applies a crop/selection rectangle request when supported by the driver. */
    bool setSelection(v4l2_selection& selection) const;

    /** @brief Forwards a `VIDIOC_REQBUFS` request to the driver. */
    bool requestBuffers(v4l2_requestbuffers& request) const;

    /** @brief Queries one capture buffer descriptor from the driver. */
    bool queryBuffer(v4l2_buffer& buffer) const;

    /** @brief Queues one prepared capture buffer back to the driver. */
    bool queueBuffer(v4l2_buffer& buffer) const;

    /** @brief Dequeues the next filled capture buffer from the driver. */
    bool dequeueBuffer(v4l2_buffer& buffer) const;

    /** @brief Enables streaming for the specified V4L2 buffer type. */
    bool streamOn(uint32_t type) const;

    /** @brief Disables streaming for the specified V4L2 buffer type. */
    bool streamOff(uint32_t type) const;

    /**
     * @brief Creates MMAP capture buffers and starts streaming capture.
     *
     * This is the high-level capture setup helper used by @ref V4L2Source. It
     * applies the requested FourCC, allocates/memory-maps buffers, queues them
     * and transitions the device to streaming mode.
     */
    bool startCapture(uint32_t requestedBufferCount);

    /** @brief Stops streaming capture and releases all mapped buffers. */
    void stopCapture();

    /** @brief Waits for one frame, dequeues a filled buffer and exposes it as @ref CaptureFrame. */
    bool captureFrame(int timeoutUs, CaptureFrame& frame);

    /** @brief Returns the cached active image format after successful setup. */
    const v4l2_format& activeFormat() const;

    /** @brief Returns @c true while streaming capture is active. */
    bool isStreaming() const;

    /** @brief Returns the active image width in pixels. */
    uint32_t activeWidth() const;

    /** @brief Returns the active image height in pixels. */
    uint32_t activeHeight() const;

    /** @brief Returns the active row stride in bytes. */
    uint32_t activeStride() const;

    /** @brief Returns the active capture FourCC. */
    uint32_t activeFourcc() const;

    /** @brief Creates and memory-maps a set of V4L2 MMAP buffers. */
    bool createMMapBuffers(uint32_t bufferType, uint32_t requestedCount, uint32_t planeCount, std::vector<MappedBuffer>& buffers) const;

    /** @brief Unmaps and clears a previously created MMAP buffer set. */
    void releaseMMapBuffers(uint32_t bufferType, std::vector<MappedBuffer>& buffers) const;

    /** @brief Waits until the device becomes readable or the timeout expires. */
    int waitReadable(int timeoutUs) const;

    /** @brief Maps one device buffer range into user space. */
    void* mmapBuffer(size_t length, off_t offset) const;

    /** @brief Unmaps one previously mapped device buffer range. */
    bool munmapBuffer(void* address, size_t length) const;

private:
    bool refreshFormat();

    int m_fd;
    bool m_ownsFd;
    std::string m_name;
    bool m_streaming;
    v4l2_format m_format;
    std::vector<MappedBuffer> m_buffers;
    unsigned int m_nextBufferIndex;
    int m_heldDequeuedBufferIndex;
};