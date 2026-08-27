// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "LogSink.hpp"

#include "core/Logger.hpp"
#include "image/ImageBuffer.hpp"
#include "image/PixelFormat.hpp"

#include <iomanip>
#include <sstream>

std::string LogSink::typeName() const
{
    return "logsink";
}

std::string LogSink::description() const
{
    return "Prints one line per received image to the console.";
}

NodeSchema LogSink::schema() const
{
    NodeSchema schema;
    schema.inputs = {NodeInputInfo{"image", "image", "Input image to log", false}};
    return schema;
}

bool LogSink::process(FrameContext& context)
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

    LOG_INFO(id() + " [#" + sequenceText.str() + ", ts=" + timestampMsText.str() + ", t=" + deltaMsText.str() + " ms, " + pixelFormatToString(image->format()) + "/" + std::to_string(image->width()) +
             "x" + std::to_string(image->height()) + "]");

    m_previousTimestampNs = timestampNs;
    m_hasPreviousTimestamp = true;
    return true;
}