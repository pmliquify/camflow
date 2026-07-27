// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "image/ImageBuffer.hpp"

#include <memory>

/**
 * @brief Abstract interface for image format converters.
 *
 * IImageConverter defines the contract for the single image format converter used
 * by the pipeline. The application injects one converter instance into nodes that
 * require image format changes.
 *
 * ### Cost model
 * The @ref cost method returns an integer priority for a specific conversion. Lower
 * values indicate a cheaper or higher-quality conversion. A return value of @c -1
 * (or any negative value) signals that this converter does not support the given
 * format pair and should not be selected.
 *
 * ### Implementing a converter
 * @code
 * class MyConverter : public IImageConverter
 * {
 * public:
 *     int cost(PixelFormat src, PixelFormat dst) const override {
 *         if (src == PixelFormat::RG10 && dst == PixelFormat::RGB888) return 1;
 *         return -1; // not supported
 *     }
 *     bool convert(const ImageBuffer& src, ImageBuffer& dst, PixelFormat dstFmt) override {
 *         // perform conversion...
 *         return true;
 *     }
 * };
 * @endcode
 *
 * @see ImageConverter
 */
class IImageConverter
{
public:
    virtual ~IImageConverter() = default;

    /**
     * @brief Returns the cost (priority) of converting @p sourceFormat to @p destinationFormat.
     *
     * A lower value means the converter is preferred over others. Negative values
     * signal that this converter cannot handle the given format combination.
     *
     * @param sourceFormat       Pixel format of the input @ref ImageBuffer.
     * @param destinationFormat  Desired output pixel format.
     * @return Non-negative cost on success; negative value if not supported.
     */
    virtual int cost(PixelFormat sourceFormat, PixelFormat destinationFormat) const = 0;

    /**
     * @brief Performs the format conversion from @p source to @p destination.
     *
     * On success @p destination contains the converted image in @p destinationFormat.
     * The dimensions of @p destination are set to match @p source.
     *
     * @param source             Input image buffer (read-only).
     * @param destination        Output image buffer (written on success).
     * @param destinationFormat  Target pixel format for the output buffer.
     * @return @c true on success; @c false if the conversion failed.
     */
    virtual bool convert(const ImageBuffer& source, ImageBuffer& destination, PixelFormat destinationFormat) = 0;
};

/// Owning smart-pointer type for @ref IImageConverter instances.
typedef std::unique_ptr<IImageConverter> IImageConverterPtr;
