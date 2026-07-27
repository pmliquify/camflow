// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "BitShiftProcessor.hpp"

#include "core/Logger.hpp"
#include "image/ImageBuffer.hpp"

namespace
{

uint8_t clampShift(int64_t value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 8) {
        return 8;
    }
    return static_cast<uint8_t>(value);
}

} // namespace

std::string BitShiftProcessor::typeName() const
{
    return "bitshift";
}

std::string BitShiftProcessor::description() const
{
    return "Applies a bit shift + BGR conversion or publishes bitshift metadata.";
}

NodeSchema BitShiftProcessor::schema() const
{
    NodeSchema schema;
    schema.parameters = {
        ParameterInfo{"shift", ParameterType::Int, "Right-shift amount", int64_t(0), int64_t(0), int64_t(8), {}, true},
        ParameterInfo{"apply", ParameterType::Bool, "Apply shift to image and convert to BGR888", true, false, true, {}, true},
    };
    schema.inputs = {NodeInputInfo{"image", "image", "Raw Bayer image", false}};
    schema.outputs = {
        NodeOutputInfo{"image", "image", "Output image (BGR when apply=true, passthrough when apply=false)"},
        NodeOutputInfo{"bitshift", "int", "Configured bitshift metadata value"},
    };
    return schema;
}

bool BitShiftProcessor::process(FrameContext& context)
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

    const uint8_t shift = clampShift(parameterInt("shift", 0));
    const bool apply = parameterBool("apply", true);

    if (!apply) {
        context.set("image", *image);
        context.set("bitshift", static_cast<int64_t>(shift));
        return true;
    }

    if (image->format() == PixelFormat::Unknown) {
        LOG_ERROR("BitShiftProcessor requires a valid input format");
        return false;
    }

    IImageConverter* imageConverter = converter();
    if (imageConverter == nullptr) {
        return false;
    }

    ImageBuffer shifted = *image;
    shifted.setBitShift(shift);

    ImageBuffer converted;
    if (!imageConverter->convert(shifted, converted, PixelFormat::BGR888)) {
        LOG_ERROR("BitShiftProcessor failed to convert image to BGR888");
        return false;
    }

    context.set("image", std::move(converted));
    return true;
}
