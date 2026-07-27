// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "image/PixelFormat.hpp"

#include <cstdint>
#include <memory>
#include <vector>

/**
 * @brief Pixel buffer that supports both owned memory and zero-copy external memory wrapping.
 *
 * ImageBuffer is the primary image data carrier in CamFlow. It can hold pixel data in
 * two modes:
 *
 * - **Owned mode**: The buffer owns a `std::vector<uint8_t>` heap allocation. Use
 *   @ref allocate to create a new allocation or @ref assign to copy external data.
 * - **Zero-copy external mode**: The buffer wraps an externally managed memory region
 *   without copying it. Use @ref wrapExternal. The lifetime of the external memory is
 *   extended via an optional `std::shared_ptr<void>` owner until the ImageBuffer is
 *   destroyed or reassigned.
 *
 * External mode is used by @ref V4L2Source to avoid copying MMAP kernel buffers:
 * the kernel buffer is wrapped with its requeue callback as the owner, and only
 * queued back to the driver when no node holds a reference to the ImageBuffer anymore.
 *
 * ### Metadata
 * Every buffer carries the following metadata fields:
 * - **Pixel format** — @ref PixelFormat enum value.
 * - **Geometry** — width, height and stride in bytes.
 * - **Bit shift** — right-shift applied before display (e.g. 2 for 10-bit packed into 16-bit).
 * - **Timestamp** — capture timestamp in nanoseconds (from the driver or pipeline clock).
 * - **Sequence number** — monotonically increasing capture sequence counter.
 *
 * @see V4L2Source
 * @see FrameContext
 * @see PixelFormat
 */
class ImageBuffer
{
public:
    /// Constructs an empty, unallocated buffer.
    ImageBuffer();

    /**
     * @brief Allocates owned heap memory for a frame of the given geometry.
     *
     * Switches to owned mode. Any previous allocation or external reference is discarded.
     * The pixel data is uninitialized after this call.
     *
     * @param width    Frame width in pixels.
     * @param height   Frame height in pixels.
     * @param stride   Row stride in bytes (>= width * bytes-per-pixel).
     * @param format   Pixel format of the data.
     */
    void allocate(uint32_t width, uint32_t height, uint32_t stride, PixelFormat format);

    /**
     * @brief Copies pixel data from an external buffer into owned heap memory.
     *
     * Switches to owned mode. Copies exactly @p size bytes from @p data.
     *
     * @param data     Pointer to the source pixel data.
     * @param size     Number of bytes to copy.
     * @param width    Frame width in pixels.
     * @param height   Frame height in pixels.
     * @param stride   Row stride in bytes.
     * @param format   Pixel format of the data.
     */
    void assign(const uint8_t* data, size_t size, uint32_t width, uint32_t height, uint32_t stride, PixelFormat format);

    /**
     * @brief Wraps an externally managed memory region without copying (zero-copy mode).
     *
     * Switches to external mode. The @p owner `shared_ptr` keeps the backing memory alive
     * as long as any copy of this ImageBuffer (or the shared_ptr) exists. When all
     * references are released the deleter of @p owner is called, which may requeue
     * a kernel buffer or perform other cleanup.
     *
     * @param data    Pointer to the start of the pixel data (must remain valid while @p owner is alive).
     * @param size    Size of the memory region in bytes.
     * @param width   Frame width in pixels.
     * @param height  Frame height in pixels.
     * @param stride  Row stride in bytes.
     * @param format  Pixel format of the data.
     * @param owner   Optional shared ownership token; its deleter is called when no longer referenced.
     */
    void wrapExternal(uint8_t* data, size_t size, uint32_t width, uint32_t height, uint32_t stride, PixelFormat format, std::shared_ptr<void> owner = nullptr);

    /** @brief Returns a mutable pointer to the first byte of pixel data. */
    uint8_t* data();

    /** @brief Returns a read-only pointer to the first byte of pixel data. */
    const uint8_t* data() const;

    /** @brief Returns the size of the pixel data in bytes. */
    size_t size() const;

    /** @brief Returns the frame width in pixels. */
    uint32_t width() const;

    /** @brief Returns the frame height in pixels. */
    uint32_t height() const;

    /** @brief Returns the row stride in bytes. */
    uint32_t stride() const;

    /** @brief Returns the pixel format. */
    PixelFormat format() const;

    /**
     * @brief Returns the bit-right-shift applied before display.
     *
     * Used for packed high-bit-depth formats (e.g. 10-bit data stored in 16-bit words).
     * A shift of 2 means each pixel value is shifted right by 2 bits before display.
     */
    uint8_t bitShift() const;

    /** @brief Sets the bit-right-shift value.
     * @param bitShift Shift amount in bits.
     */
    void setBitShift(uint8_t bitShift);

    /**
     * @brief Sets the capture timestamp.
     * @param timestampNs Capture time in nanoseconds since some epoch (driver-defined).
     */
    void setTimestampNs(uint64_t timestampNs);

    /** @brief Returns the capture timestamp in nanoseconds. */
    uint64_t timestampNs() const;

    /**
     * @brief Sets the capture sequence number.
     * @param sequence Monotonically increasing frame index.
     */
    void setSequence(uint64_t sequence);

    /** @brief Returns the capture sequence number. */
    uint64_t sequence() const;

private:
    std::vector<uint8_t> m_data;           ///< Owned heap allocation (empty in external mode).
    uint8_t* m_externalData;               ///< Pointer to external pixel data (nullptr in owned mode).
    size_t m_externalSize;                 ///< Size of external memory region in bytes.
    std::shared_ptr<void> m_externalOwner; ///< Lifetime token for the external memory region.
    uint32_t m_width;                      ///< Frame width in pixels.
    uint32_t m_height;                     ///< Frame height in pixels.
    uint32_t m_stride;                     ///< Row stride in bytes.
    PixelFormat m_format;                  ///< Pixel format descriptor.
    uint8_t m_bitShift;                    ///< Right-shift for display of high-bit-depth formats.
    uint64_t m_timestampNs;                ///< Capture timestamp in nanoseconds.
    uint64_t m_sequence;                   ///< Capture sequence counter.
};
