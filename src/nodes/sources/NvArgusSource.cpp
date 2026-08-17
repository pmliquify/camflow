// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "NvArgusSource.hpp"

#include "core/Logger.hpp"
#include "image/ImageBuffer.hpp"
#include "image/PixelFormat.hpp"

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include <string>
#include <vector>

NvArgusSource::NvArgusSource() :
    m_pipeline(nullptr),
    m_sink(nullptr),
    m_sequence(0)
{
    gst_init(nullptr, nullptr);
}

NvArgusSource::~NvArgusSource()
{
    shutdown();
}

std::string NvArgusSource::typeName() const
{
    return "nvargussrc";
}

std::string NvArgusSource::description() const
{
    return "Captures NV12 frames from NVIDIA nvarguscamerasrc through GStreamer.";
}

NodeSchema NvArgusSource::schema() const
{
    NodeSchema schema;
    schema.parameters = {{"sensorId", ParameterType::Int, "NvArgus sensor id", int64_t(0), int64_t(0), int64_t(16), {}, false},
                         {"aeLock", ParameterType::Bool, "Enable auto exposure lock", false, false, true, {}, true},
                         {"aeLeft", ParameterType::Int, "Auto exposure region left", int64_t(0), int64_t(0), int64_t(1000000), {}, true},
                         {"aeTop", ParameterType::Int, "Auto exposure region top", int64_t(0), int64_t(0), int64_t(1000000), {}, true},
                         {"aeWidth", ParameterType::Int, "Auto exposure region width; 0 means full frame", int64_t(0), int64_t(0), int64_t(1000000), {}, true},
                         {"aeHeight", ParameterType::Int, "Auto exposure region height; 0 means full frame", int64_t(0), int64_t(0), int64_t(1000000), {}, true},
                         {"gainRange", ParameterType::Int, "Maximum analog gain value for gainrange", int64_t(1), int64_t(1), int64_t(1000), {}, true},
                         {"ispDigitalGainRange", ParameterType::Int, "Maximum ISP digital gain value", int64_t(1), int64_t(1), int64_t(1000), {}, true},
                         {"awbLock", ParameterType::Bool, "Enable auto white balance lock", false, false, true, {}, true},
                         {"wbMode", ParameterType::Int, "NvArgus white balance mode", int64_t(1), int64_t(0), int64_t(9), {}, true},
                         {"tnrMode", ParameterType::Int, "Temporal noise reduction mode", int64_t(1), int64_t(0), int64_t(3), {}, true},
                         {"width", ParameterType::Int, "Output width", int64_t(1024), int64_t(1), int64_t(1000000), {}, false},
                         {"height", ParameterType::Int, "Output height", int64_t(768), int64_t(1), int64_t(1000000), {}, false},
                         {"frameRate", ParameterType::Int, "Frame rate", int64_t(20), int64_t(1), int64_t(240), {}, false}};
    schema.outputs = {NodeOutputInfo{"image", "image", "Captured NV12 frame"}};
    return schema;
}

std::string NvArgusSource::pipelineDescription() const
{
    const int sensorId = static_cast<int>(parameterInt("sensorId", 0));
    const bool aeLock = parameterBool("aeLock", false);
    const int aeLeft = static_cast<int>(parameterInt("aeLeft", 0));
    const int aeTop = static_cast<int>(parameterInt("aeTop", 0));
    const int aeWidth = static_cast<int>(parameterInt("aeWidth", 0));
    const int aeHeight = static_cast<int>(parameterInt("aeHeight", 0));
    const int gainRange = static_cast<int>(parameterInt("gainRange", 1));
    const int ispDigitalGainRange = static_cast<int>(parameterInt("ispDigitalGainRange", 1));
    const bool awbLock = parameterBool("awbLock", false);
    const int wbMode = static_cast<int>(parameterInt("wbMode", 1));
    const int tnrMode = static_cast<int>(parameterInt("tnrMode", 1));
    const int width = static_cast<int>(parameterInt("width", 1024));
    const int height = static_cast<int>(parameterInt("height", 768));
    const int frameRate = static_cast<int>(parameterInt("frameRate", 20));

    std::string aeRegion;
    if (aeWidth > 0 && aeHeight > 0) {
        aeRegion = std::to_string(aeLeft) + " " + std::to_string(aeTop) + " " + std::to_string(aeLeft + aeWidth) + " " + std::to_string(aeTop + aeHeight) + " 1";
    }

    return "nvarguscamerasrc sensor-id=" + std::to_string(sensorId) + " aelock=" + (aeLock ? std::string("true") : std::string("false")) +
           (aeRegion.empty() ? std::string() : " aeregion=\"" + aeRegion + "\"") + " gainrange=\"1 " + std::to_string(gainRange) + "\"" + " ispdigitalgainrange=\"1 " +
           std::to_string(ispDigitalGainRange) + "\"" + " awblock=" + (awbLock ? std::string("true") : std::string("false")) + " wbmode=" + std::to_string(wbMode) +
           " tnr-mode=" + std::to_string(tnrMode) + " ! " + "video/x-raw(memory:NVMM), width=" + std::to_string(width) + ", height=" + std::to_string(height) +
           ", format=NV12, framerate=" + std::to_string(frameRate) +
           "/1 ! "
           "nvvidconv ! "
           "video/x-raw, format=NV12, width=" +
           std::to_string(width) + ", height=" + std::to_string(height) + " ! appsink name=sink emit-signals=false sync=false max-buffers=2 drop=true";
}

bool NvArgusSource::init()
{
    std::string description = pipelineDescription();
    LOG_INFO("GStreamer pipeline: " + description);

    GError* error = nullptr;
    m_pipeline = gst_parse_launch(description.c_str(), &error);
    if (!m_pipeline) {
        LOG_ERROR(std::string("Could not create NvArgus GStreamer pipeline: ") + (error ? error->message : "unknown error"));
        if (error) {
            g_error_free(error);
        }
        return false;
    }

    m_sink = gst_bin_get_by_name(GST_BIN(m_pipeline), "sink");
    if (!m_sink) {
        LOG_ERROR("Could not find appsink named sink in NvArgus pipeline");
        shutdown();
        return false;
    }

    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    return true;
}

bool NvArgusSource::process(FrameContext& context)
{
    if (!m_sink) {
        return false;
    }

    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(m_sink));
    if (!sample) {
        LOG_WARNING("NvArgusSource received no sample");
        return false;
    }

    GstCaps* caps = gst_sample_get_caps(sample);
    GstVideoInfo videoInfo;
    gst_video_info_from_caps(&videoInfo, caps);

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (!buffer) {
        gst_sample_unref(sample);
        return false;
    }

    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return false;
    }

    uint32_t width = GST_VIDEO_INFO_WIDTH(&videoInfo);
    uint32_t height = GST_VIDEO_INFO_HEIGHT(&videoInfo);
    uint32_t stride = GST_VIDEO_INFO_PLANE_STRIDE(&videoInfo, 0);
    uint64_t timestampNs = GST_BUFFER_PTS(buffer);

    ImageBuffer image;
    image.assign(map.data, map.size, width, height, stride, PixelFormat::NV12);
    image.setSequence(++m_sequence);
    image.setTimestampNs(timestampNs);
    context.set("image", std::move(image));

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);
    return true;
}

void NvArgusSource::shutdown()
{
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
    }
    if (m_sink) {
        gst_object_unref(m_sink);
        m_sink = nullptr;
    }
    if (m_pipeline) {
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }
}
