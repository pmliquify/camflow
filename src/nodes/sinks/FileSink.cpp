// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "FileSink.hpp"

#include "core/Logger.hpp"
#include "image/ImageBuffer.hpp"

#include <opencv2/imgcodecs.hpp>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{

std::string extensionForFileFormat(const std::string& value)
{
    if (value == "png") {
        return ".png";
    }
    if (value == "jpg") {
        return ".jpg";
    }
    return ".raw";
}

std::string currentWriteDatetime()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    localtime_r(&time, &localTime);

    std::ostringstream stream;
    stream << std::put_time(&localTime, "%Y%m%d_%H%M%S");
    return stream.str();
}

void ensureParentDirectoryExists(const std::string& fileName)
{
    const std::filesystem::path parent = std::filesystem::path(fileName).parent_path();
    if (parent.empty()) {
        return;
    }
    std::error_code error;
    std::filesystem::create_directories(parent, error);
    if (error) {
        LOG_ERROR("FileSink could not create output directory: " + parent.string() + " (" + error.message() + ")");
    }
}

} // namespace

std::string FileSink::typeName() const
{
    return "filesink";
}

std::string FileSink::description() const
{
    return "Writes image buffers to files, including PNG and JPG output.";
}

NodeSchema FileSink::schema() const
{
    NodeSchema schema;
    schema.parameters = {{"filename", ParameterType::String, "Output file name without file extension", std::string("images/image"), std::string(), std::string(), {}, true},
                         {"appendDatetime", ParameterType::Bool, "Append write date/time (YYYYMMDD_hhmmss)", true, false, true, {}, true},
                         {"appendSequence", ParameterType::Bool, "Append frame sequence number", true, false, true, {}, true},
                         {"appendPixelFormat", ParameterType::Bool, "Append the written image's pixel format", true, false, true, {}, true},
                         {"appendImageSize", ParameterType::Bool, "Append image size as <width>x<height>", true, false, true, {}, true},
                         {"format", ParameterType::Option, "Output file format", std::string("jpg"), std::string(), std::string(), {"jpg", "png", "raw"}, true}};
    schema.inputs = {NodeInputInfo{"image", "image", "Input image", false}};
    return schema;
}

std::string FileSink::outputFileName(const std::string& baseFileName, const std::string& extension, bool appendDatetime, bool appendSequence, bool appendPixelFormat, bool appendImageSize,
                                     uint64_t sequence, const std::string& pixelFormatName, uint32_t width, uint32_t height) const
{
    std::ostringstream stream;
    stream << baseFileName;
    if (appendDatetime) {
        stream << "_" << currentWriteDatetime();
    }
    if (appendSequence) {
        stream << "_" << sequence;
    }
    if (appendPixelFormat) {
        stream << "_" << pixelFormatName;
    }
    if (appendImageSize) {
        stream << "_" << width << "x" << height;
    }
    stream << extension;
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

    const std::string formatValue = parameterString("format", "jpg");
    const std::string baseFileName = parameterString("filename", "images/image");
    const bool encodedTarget = formatValue == "png" || formatValue == "jpg";
    const std::string fileExtension = extensionForFileFormat(formatValue);
    const bool appendDatetime = parameterBool("appendDatetime", true);
    const bool appendSequence = parameterBool("appendSequence", true);
    const bool appendPixelFormat = parameterBool("appendPixelFormat", true);
    const bool appendImageSize = parameterBool("appendImageSize", true);

    ImageBuffer convertedImage;
    const ImageBuffer* writeImage = image;

    if (encodedTarget) {
        IImageConverter* imageConverter = converter();
        if (imageConverter == nullptr) {
            return false;
        }

        if (image->format() != PixelFormat::BGR888) {
            if (!imageConverter->convert(*image, convertedImage, PixelFormat::BGR888)) {
                LOG_ERROR("FileSink failed to convert source image to BGR888 for encoded output");
                return false;
            }
            writeImage = &convertedImage;
        }

        const std::string fileName = outputFileName(baseFileName, fileExtension, appendDatetime, appendSequence, appendPixelFormat, appendImageSize, image->sequence(),
                                                    pixelFormatToString(writeImage->format()), writeImage->width(), writeImage->height());
        ensureParentDirectoryExists(fileName);

        cv::Mat bgr(static_cast<int>(writeImage->height()), static_cast<int>(writeImage->width()), CV_8UC3, const_cast<uint8_t*>(writeImage->data()), static_cast<size_t>(writeImage->stride()));
        if (!cv::imwrite(fileName, bgr)) {
            LOG_ERROR("Could not write encoded output file: " + fileName);
            return false;
        }
        LOG_INFO("Wrote encoded image file: " + fileName);
        return true;
    }

    // RAW output is a byte-for-byte dump of the in-memory buffer, so the file size always matches width * height * bytesPerPixel.
    const std::string fileName = outputFileName(baseFileName, fileExtension, appendDatetime, appendSequence, appendPixelFormat, appendImageSize, writeImage->sequence(),
                                                pixelFormatToString(writeImage->format()), writeImage->width(), writeImage->height());
    ensureParentDirectoryExists(fileName);

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
