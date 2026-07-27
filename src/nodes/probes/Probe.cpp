// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "Probe.hpp"

#include "core/Logger.hpp"
#include "image/ImageBuffer.hpp"
#include "image/PixelFormat.hpp"

#include <iomanip>
#include <sstream>

std::string Probe::typeName() const
{
    return "probe";
}

std::string Probe::description() const
{
    return "Prints one line per processed image to the console.";
}

NodeSchema Probe::schema() const
{
    NodeSchema schema;
    schema.inputs = {NodeInputInfo{"image", "image", "Input image to inspect", false}};
    schema.outputs = {NodeOutputInfo{"image", "image", "Forwarded image"}};
    return schema;
}

bool Probe::process(FrameContext& context)
{
    const ImageBuffer* image = context.get<ImageBuffer>("image");
    if (image == nullptr) {
        return false;
    }

    const uint64_t timestampNs = image->timestampNs();

    std::ostringstream sequenceText;
    sequenceText << std::setw(4) << std::setfill('0') << image->sequence();

    std::ostringstream timestampMsText;
    timestampMsText << std::setw(8) << (timestampNs / 1000000ull);

    std::ostringstream deltaMsText;
    if (m_hasPreviousTimestamp && timestampNs >= m_previousTimestampNs) {
        deltaMsText << std::setw(4) << ((timestampNs - m_previousTimestampNs) / 1000000ull);
    } else {
        deltaMsText << std::setw(4) << "n/a";
    }

    const std::string probeId = id();
    LOG_INFO(probeId + " [#" + sequenceText.str() + ", ts=" + timestampMsText.str() + ", t=" + deltaMsText.str() + " ms, " + pixelFormatToString(image->format()) + "/" +
             std::to_string(image->width()) + "x" + std::to_string(image->height()) + "]");

    m_previousTimestampNs = timestampNs;
    m_hasPreviousTimestamp = true;
    return true;
}
