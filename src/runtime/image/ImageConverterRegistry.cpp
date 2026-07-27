// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "ImageConverterRegistry.hpp"

#include "core/Logger.hpp"

void ImageConverterRegistry::registerConverter(IImageConverterPtr converter)
{
    m_converters.push_back(std::move(converter));
}

bool ImageConverterRegistry::canConvert(PixelFormat sourceFormat, PixelFormat destinationFormat) const
{
    for (const auto& converter : m_converters) {
        if (converter->cost(sourceFormat, destinationFormat) >= 0) {
            return true;
        }
    }
    return false;
}

bool ImageConverterRegistry::convert(const ImageBuffer& source, ImageBuffer& destination, PixelFormat destinationFormat) const
{
    IImageConverter* bestConverter = nullptr;
    int bestCost = -1;
    for (const auto& converter : m_converters) {
        int converterCost = converter->cost(source.format(), destinationFormat);
        if (converterCost >= 0 && (bestCost < 0 || converterCost < bestCost)) {
            bestConverter = converter.get();
            bestCost = converterCost;
        }
    }
    if (bestConverter == nullptr) {
        LOG_ERROR("No image converter for " + pixelFormatToString(source.format()) + " -> " + pixelFormatToString(destinationFormat));
        return false;
    }
    return bestConverter->convert(source, destination, destinationFormat);
}
