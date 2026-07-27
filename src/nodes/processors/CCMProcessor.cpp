// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "CCMProcessor.hpp"

#include "core/Logger.hpp"
#include "image/ImageBuffer.hpp"

#include <opencv2/core.hpp>

std::string CCMProcessor::typeName() const
{
    return "ccm";
}

std::string CCMProcessor::description() const
{
    return "Applies a 3x3 color correction matrix to BGR output; auto-debayers RAW Bayer input.";
}

NodeSchema CCMProcessor::schema() const
{
    NodeSchema schema;
    schema.parameters = {
        {"m00", ParameterType::Double, "CCM row 0 col 0", 1.0, -8.0, 8.0, {}, false}, {"m01", ParameterType::Double, "CCM row 0 col 1", 0.0, -8.0, 8.0, {}, false},
        {"m02", ParameterType::Double, "CCM row 0 col 2", 0.0, -8.0, 8.0, {}, false}, {"m10", ParameterType::Double, "CCM row 1 col 0", 0.0, -8.0, 8.0, {}, false},
        {"m11", ParameterType::Double, "CCM row 1 col 1", 1.0, -8.0, 8.0, {}, false}, {"m12", ParameterType::Double, "CCM row 1 col 2", 0.0, -8.0, 8.0, {}, false},
        {"m20", ParameterType::Double, "CCM row 2 col 0", 0.0, -8.0, 8.0, {}, false}, {"m21", ParameterType::Double, "CCM row 2 col 1", 0.0, -8.0, 8.0, {}, false},
        {"m22", ParameterType::Double, "CCM row 2 col 2", 1.0, -8.0, 8.0, {}, false},
    };
    schema.inputs = {NodeInputInfo{"image", "image", "Input image", false}};
    schema.outputs = {NodeOutputInfo{"image", "image", "Color corrected image"}};
    return schema;
}

bool CCMProcessor::process(FrameContext& context)
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

    const ImageBuffer* bgrInput = image;
    IImageConverter* imageConverter = converter();
    if (imageConverter == nullptr) {
        return false;
    }
    ImageBuffer converted;
    if (image->format() != PixelFormat::BGR888) {
        if (!imageConverter->convert(*image, converted, PixelFormat::BGR888)) {
            LOG_ERROR("CCMProcessor failed to convert input to BGR888");
            return false;
        }
        bgrInput = &converted;
    }

    const cv::Mat src(static_cast<int>(bgrInput->height()), static_cast<int>(bgrInput->width()), CV_8UC3, const_cast<uint8_t*>(bgrInput->data()), bgrInput->stride());
    cv::Mat srcFloat;
    src.convertTo(srcFloat, CV_32FC3);

    cv::Matx33f matrix(static_cast<float>(parameterDouble("m00", 1.0)), static_cast<float>(parameterDouble("m01", 0.0)), static_cast<float>(parameterDouble("m02", 0.0)),
                       static_cast<float>(parameterDouble("m10", 0.0)), static_cast<float>(parameterDouble("m11", 1.0)), static_cast<float>(parameterDouble("m12", 0.0)),
                       static_cast<float>(parameterDouble("m20", 0.0)), static_cast<float>(parameterDouble("m21", 0.0)), static_cast<float>(parameterDouble("m22", 1.0)));

    cv::Mat correctedFloat;
    cv::transform(srcFloat, correctedFloat, matrix);

    ImageBuffer output;
    output.allocate(bgrInput->width(), bgrInput->height(), bgrInput->width() * 3, PixelFormat::BGR888);
    cv::Mat dst(static_cast<int>(output.height()), static_cast<int>(output.width()), CV_8UC3, output.data(), output.stride());
    correctedFloat.convertTo(dst, CV_8UC3);
    output.setSequence(image->sequence());
    output.setTimestampNs(image->timestampNs());

    context.set("image", std::move(output));
    return true;
}
