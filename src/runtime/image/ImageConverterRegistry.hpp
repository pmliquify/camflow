// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "image/IImageConverter.hpp"

#include <vector>

/**
 * @brief Legacy registry and dispatcher for @ref IImageConverter implementations.
 *
 * This type remains for backwards compatibility, but the active runtime now
 * injects a single @ref IImageConverter instance directly into nodes. New code
 * should prefer @ref IImageConverter and @ref Node::setImageConverter.
 *
 * @see IImageConverter
 * @see Node::setImageConverter
 */
class ImageConverterRegistry
{
public:
    /**
     * @brief Registers a converter and takes ownership of it.
     * @param converter Owning pointer to the converter to register.
     */
    void registerConverter(IImageConverterPtr converter);

    /**
     * @brief Checks whether any registered converter supports the given format pair.
     *
     * @param sourceFormat       Pixel format of the input image.
     * @param destinationFormat  Desired output pixel format.
     * @return @c true if at least one registered converter can handle the conversion.
     */
    bool canConvert(PixelFormat sourceFormat, PixelFormat destinationFormat) const;

    /**
     * @brief Converts @p source to @p destinationFormat using the best available converter.
     *
     * Selects the registered converter with the lowest non-negative cost for the
     * given format pair and calls its @ref IImageConverter::convert method.
     *
     * @param source             Read-only input image buffer.
     * @param destination        Output image buffer; filled with converted data on success.
     * @param destinationFormat  Target pixel format.
     * @return @c true if a suitable converter was found and the conversion succeeded;
     *         @c false if no converter supports the format pair or the conversion failed.
     */
    bool convert(const ImageBuffer& source, ImageBuffer& destination, PixelFormat destinationFormat) const;

private:
    std::vector<IImageConverterPtr> m_converters; ///< All registered converters.
};
