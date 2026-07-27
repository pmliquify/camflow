// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "FileSink.hpp"

#include "core/Logger.hpp"
#include "image/ImageBuffer.hpp"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace
{

bool endsWithCaseInsensitive(const std::string& value, const std::string& suffix)
{
    if (value.size() < suffix.size()) {
        return false;
    }
    const size_t start = value.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i) {
        const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(value[start + i])));
        const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(suffix[i])));
        if (a != b) {
            return false;
        }
    }
    return true;
}

} // namespace

std::string FileSink::typeName() const
{
    return "filesink";
}

std::string FileSink::description() const
{
    return "Writes image buffers to files, including PNG and JPEG output.";
}

NodeSchema FileSink::schema() const
{
    NodeSchema schema;
    schema.parameters = {{"file", ParameterType::String, "Output file name", std::string("out.raw"), std::string(), std::string(), {}, true},
                         {"format", ParameterType::String, "Optional target RAW format (e.g. RG14P, RG14, GREY, Y14, YUYV, NV12)", std::string(), std::string(), std::string(), {}, false},
                         {"appendSequence", ParameterType::Bool, "Append image sequence number before the extension", false, false, true, {}, true},
                         {"appendTimestamp", ParameterType::Bool, "Append image timestamp in ns before the extension", false, false, true, {}, true}};
    schema.inputs = {NodeInputInfo{"image", "image", "Input image", false}};
    return schema;
}

bool FileSink::isEncodedTarget(const std::string& fileName) const
{
    return endsWithCaseInsensitive(fileName, ".png") || endsWithCaseInsensitive(fileName, ".jpg") || endsWithCaseInsensitive(fileName, ".jpeg");
}

PixelFormat FileSink::parseFormatString(const std::string& value) const
{
    if (value.empty()) {
        return PixelFormat::Unknown;
    }
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    if (normalized == "RGGB") {
        return PixelFormat::RG8;
    }
    if (normalized == "GBRG") {
        return PixelFormat::GB8;
    }
    if (normalized == "GRBG") {
        return PixelFormat::GR8;
    }
    if (normalized == "BGGR") {
        return PixelFormat::BG8;
    }
    if (normalized == "RG10") {
        return PixelFormat::RG10;
    }
    if (normalized == "GB10") {
        return PixelFormat::GB10;
    }
    if (normalized == "GR10") {
        return PixelFormat::GR10;
    }
    if (normalized == "BG10") {
        return PixelFormat::BG10;
    }
    if (normalized == "RG10P") {
        return PixelFormat::RG10P;
    }
    if (normalized == "GB10P") {
        return PixelFormat::GB10P;
    }
    if (normalized == "GR10P") {
        return PixelFormat::GR10P;
    }
    if (normalized == "BG10P") {
        return PixelFormat::BG10P;
    }
    if (normalized == "RG12") {
        return PixelFormat::RG12;
    }
    if (normalized == "GB12") {
        return PixelFormat::GB12;
    }
    if (normalized == "GR12") {
        return PixelFormat::GR12;
    }
    if (normalized == "BG12") {
        return PixelFormat::BG12;
    }
    if (normalized == "RG12P") {
        return PixelFormat::RG12P;
    }
    if (normalized == "GB12P") {
        return PixelFormat::GB12P;
    }
    if (normalized == "GR12P") {
        return PixelFormat::GR12P;
    }
    if (normalized == "BG12P") {
        return PixelFormat::BG12P;
    }
    if (normalized == "RG14") {
        return PixelFormat::RG14;
    }
    if (normalized == "GB14") {
        return PixelFormat::GB14;
    }
    if (normalized == "GR14") {
        return PixelFormat::GR14;
    }
    if (normalized == "BG14") {
        return PixelFormat::BG14;
    }
    if (normalized == "RG14P") {
        return PixelFormat::RG14P;
    }
    if (normalized == "GB14P") {
        return PixelFormat::GB14P;
    }
    if (normalized == "GR14P") {
        return PixelFormat::GR14P;
    }
    if (normalized == "BG14P") {
        return PixelFormat::BG14P;
    }
    if (normalized == "Y10" || normalized == "Y10P") {
        return PixelFormat::Mono10;
    }
    if (normalized == "Y12" || normalized == "Y12P") {
        return PixelFormat::Mono12;
    }
    if (normalized == "Y14") {
        return PixelFormat::Mono14;
    }
    return pixelFormatFromString(normalized);
}

std::string FileSink::outputFileName(const std::string& baseFileName, bool appendSequence, bool appendTimestamp, uint64_t sequence, uint64_t timestampNs) const
{
    if (!appendSequence && !appendTimestamp) {
        return baseFileName;
    }
    auto dot = baseFileName.find_last_of('.');
    std::string base = dot == std::string::npos ? baseFileName : baseFileName.substr(0, dot);
    std::string ext = dot == std::string::npos ? std::string() : baseFileName.substr(dot);
    std::ostringstream stream;
    stream << base;
    if (appendSequence) {
        stream << "_seq" << sequence;
    }
    if (appendTimestamp) {
        stream << "_ts" << timestampNs;
    }
    stream << ext;
    return stream.str();
}

bool FileSink::process(FrameContext& context)
{
    const auto bindings = resolveInputBindings("image", context);
    ImageBuffer* image = nullptr;
    for (const auto& binding : bindings) {
        image = binding.first.empty() ? context.get<ImageBuffer>(binding.second) : context.get<ImageBuffer>(binding.first, binding.second);
        if (image != nullptr) {
            break;
        }
    }
    if (image == nullptr) {
        return false;
    }

    const std::string baseFileName = parameterString("file", "out.raw");
    const std::string formatValue = parameterString("format", std::string());
    const PixelFormat outputFormat = formatValue.empty() ? PixelFormat::Unknown : parseFormatString(formatValue);
    const bool appendSequence = parameterBool("appendSequence", false);
    const bool appendTimestamp = parameterBool("appendTimestamp", false);

    std::string fileName = outputFileName(baseFileName, appendSequence, appendTimestamp, image->sequence(), image->timestampNs());

    ImageBuffer convertedImage;

    if (isEncodedTarget(fileName)) {
        IImageConverter* imageConverter = converter();
        if (imageConverter == nullptr) {
            return false;
        }

        const ImageBuffer* writeImage = image;
        if (image->format() != PixelFormat::BGR888) {
            if (!imageConverter->convert(*image, convertedImage, PixelFormat::BGR888)) {
                LOG_ERROR("FileSink failed to convert source image to BGR888 for encoded output: " + fileName);
                return false;
            }
            writeImage = &convertedImage;
        }

        cv::Mat bgr(static_cast<int>(writeImage->height()), static_cast<int>(writeImage->width()), CV_8UC3, const_cast<uint8_t*>(writeImage->data()), static_cast<size_t>(writeImage->stride()));
        if (!cv::imwrite(fileName, bgr)) {
            LOG_ERROR("Could not write encoded output file: " + fileName);
            return false;
        }
        LOG_INFO("Wrote encoded image file: " + fileName);
        return true;
    }

    const ImageBuffer* writeImage = image;
    if (outputFormat != PixelFormat::Unknown && image->format() != outputFormat) {
        IImageConverter* imageConverter = converter();
        if (imageConverter == nullptr) {
            return false;
        }
        if (!imageConverter->convert(*image, convertedImage, outputFormat)) {
            LOG_ERROR("FileSink failed to convert to requested output format: " + pixelFormatToString(outputFormat));
            return false;
        }
        writeImage = &convertedImage;
    }

    std::ofstream file(fileName, std::ios::binary);
    if (!file) {
        LOG_ERROR("Could not open output file: " + fileName);
        return false;
    }
    file.write(reinterpret_cast<const char*>(writeImage->data()), static_cast<std::streamsize>(writeImage->size()));
    if (!file) {
        LOG_ERROR("Could not write output file: " + fileName);
        return false;
    }
    LOG_INFO("Wrote raw image file: " + fileName);
    return true;
}
