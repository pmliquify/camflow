// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "DebayerProcessor.hpp"

#include "core/Logger.hpp"
#include "image/ImageBuffer.hpp"

std::string DebayerProcessor::typeName() const
{
    return "debayer";
}

std::string DebayerProcessor::description() const
{
    return "Debayers Bayer RAW input to BGR888.";
}

NodeSchema DebayerProcessor::schema() const
{
    NodeSchema schema;
    schema.inputs = {NodeInputInfo{"image", "image", "Input RAW Bayer image", false}};
    schema.outputs = {NodeOutputInfo{"image", "image", "Debayered BGR image"}};
    return schema;
}

bool DebayerProcessor::process(FrameContext& context)
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
    if (!isBayerFormat(image->format())) {
        LOG_ERROR("DebayerProcessor only accepts Bayer RAW formats");
        return false;
    }
    IImageConverter* imageConverter = converter();
    if (imageConverter == nullptr) {
        return false;
    }
    ImageBuffer converted;
    if (!imageConverter->debayer(*image, converted)) {
        LOG_ERROR("DebayerProcessor failed to convert image to BGR888");
        return false;
    }

    context.set("image", std::move(converted));
    return true;
}
