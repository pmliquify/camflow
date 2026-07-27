// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "V4L2Source.hpp"

#include "core/Logger.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fcntl.h>

static uint32_t fourccFromString(const std::string& value)
{
    if (value.size() != 4) {
        return 0;
    }
    return static_cast<uint32_t>(value[0]) | (static_cast<uint32_t>(value[1]) << 8) | (static_cast<uint32_t>(value[2]) << 16) | (static_cast<uint32_t>(value[3]) << 24);
}

static std::vector<std::string> listDeviceEntriesWithPrefix(const std::string& prefix)
{
    std::vector<std::string> result;
    const std::filesystem::path devRoot("/dev");
    std::error_code ec;
    if (!std::filesystem::exists(devRoot, ec)) {
        return result;
    }

    for (const auto& entry : std::filesystem::directory_iterator(devRoot, ec)) {
        if (ec) {
            break;
        }
        const std::string name = entry.path().filename().string();
        if (name.rfind(prefix, 0) != 0) {
            continue;
        }
        result.push_back(entry.path().string());
    }

    std::sort(result.begin(), result.end());
    return result;
}

static bool isFormatParameter(const std::string& parameterName)
{
    return parameterName == "pixelformat" || parameterName == "width" || parameterName == "height";
}

static bool isDeviceSelectionParameter(const std::string& parameterName)
{
    return parameterName == "device" || parameterName == "subdevice";
}

static std::string driverVersionString(uint32_t packedVersion)
{
    const uint32_t major = (packedVersion >> 16) & 0xffu;
    const uint32_t minor = (packedVersion >> 8) & 0xffu;
    const uint32_t patch = packedVersion & 0xffu;
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

V4L2Source::V4L2Source() :
    m_pixelFormat(PixelFormat::RG10),
    m_width(640),
    m_height(480),
    m_stride(1280),
    m_bufferCount(3),
    m_timeoutUs(1000000),
    m_sequence(0),
    m_device(),
    m_subDevice()
{
}

V4L2Source::~V4L2Source()
{
    shutdown();
}

std::string V4L2Source::typeName() const
{
    return "v4l2src";
}

std::string V4L2Source::description() const
{
    return "Captures images from a Linux V4L2 video device.";
}

NodeSchema V4L2Source::schema() const
{
    NodeSchema schema;
    ParameterSchema result = {{"device", ParameterType::Option, "V4L2 device", std::string("/dev/video0"), std::string(), std::string(), {}, false},
                              {"subdevice", ParameterType::Option, "V4L2 subdevice", std::string(), std::string(), std::string(), {}, false},
                              {"pixelformat", ParameterType::Option, "Capture pixel format reported by the V4L2 device", std::string("RG10"), std::string(), std::string(), {}, false},
                              {"width", ParameterType::Int, "Capture frame width in pixels", int64_t(640), int64_t(1), int64_t(8192), {}, false},
                              {"height", ParameterType::Int, "Capture frame height in pixels", int64_t(480), int64_t(1), int64_t(8192), {}, false}};

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        refreshCurrentParameterValues();
        result[0].hasSideEffects = true;
        result[1].hasSideEffects = true;
        result[0].options = m_deviceOptions;
        result[0].optionLabels = m_deviceOptionLabels;
        result[1].options = m_subDeviceOptions;
        result[1].optionLabels = m_subDeviceOptionLabels;
        result[2].options = m_formatOptions;
        result[2].optionLabels = m_formatOptionLabels;
        result[3].defaultValue = static_cast<int64_t>(m_width);
        result[4].defaultValue = static_cast<int64_t>(m_height);
        result.insert(result.end(), m_controlSchema.begin(), m_controlSchema.end());
    }
    schema.parameters = std::move(result);
    schema.outputs = {NodeOutputInfo{"image", "image", "Captured frame"}};
    return schema;
}

bool V4L2Source::configure(const ParameterSet& parameters)
{
    m_explicitParameters.clear();
    for (const auto& item : parameters.values()) {
        m_explicitParameters.insert(item.first);
    }

    Node::configure(parameters);

    std::lock_guard<std::mutex> lock(m_mutex);
    refreshDeviceOptions();
    refreshFormatOptions();
    if (isExplicitParameter("width")) {
        m_width = static_cast<uint32_t>(parameterInt("width", static_cast<int64_t>(m_width)));
    }
    if (isExplicitParameter("height")) {
        m_height = static_cast<uint32_t>(parameterInt("height", static_cast<int64_t>(m_height)));
    }
    if (isExplicitParameter("pixelformat")) {
        m_pixelFormat = pixelFormatFromString(parameterString("pixelformat", pixelFormatToString(m_pixelFormat)));
    }

    refreshControlSchema();
    return true;
}

bool V4L2Source::init()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    refreshDeviceOptions();
    refreshFormatOptions();
    if (!openDevice()) {
        refreshControlSchema();
        return true;
    }
    applyRequestedFormat();
    updateImageGeometry();
    refreshControlSchema();
    applyConfiguredControls();

    return true;
}

bool V4L2Source::start()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_device.isOpen()) {
        return true;
    }
    if (m_device.isStreaming()) {
        return true;
    }

    refreshControlSchema();
    applyConfiguredControls();

    if (!startCapture()) {
        LOG_WARNING("Could not start capture. Falling back to generated test frames.");
        return true;
    }
    updateImageGeometry();
    refreshControlSchema();
    return true;
}

bool V4L2Source::process(FrameContext& context)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_device.isStreaming()) {
        V4L2Device::CaptureFrame frame;
        if (!m_device.captureFrame(static_cast<int>(m_timeoutUs), frame)) {
            return false;
        }
        ImageBuffer image;
        image.wrapExternal(frame.data, frame.size, frame.width, frame.height, frame.stride, pixelFormatFromFourCC(frame.fourcc, m_pixelFormat));

        image.setSequence(frame.sequence);
        image.setTimestampNs(frame.timestampNs);
        context.set("image", std::move(image));
        return true;
    }
    return generateTestFrame(context);
}

void V4L2Source::onParameterChanged(const std::string& name, const ParameterValue& value, const ParameterValue*)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_explicitParameters.insert(name);

    if (!m_device.isOpen()) {
        return;
    }

    if (isDeviceSelectionParameter(name)) {
        // Device path changes are stored and become effective with the next
        // full device open sequence (init/shutdown cycle).
        refreshDeviceOptions();
        refreshFormatOptions();
        return;
    }

    if (isFormatParameter(name)) {
        applyRequestedFormat();
        updateImageGeometry();
        refreshControlSchema();
        return;
    }

    applyControlParameterLocked(name, value);
}

bool V4L2Source::applyControlParameterLocked(const std::string& name, const ParameterValue& value)
{
    auto it = m_controlByParameter.find(name);
    if (it == m_controlByParameter.end() || !m_device.isOpen()) {
        return false;
    }

    int64_t controlValue = 0;
    if (std::holds_alternative<int64_t>(value)) {
        controlValue = std::get<int64_t>(value);
    } else if (std::holds_alternative<bool>(value)) {
        controlValue = std::get<bool>(value) ? 1 : 0;
    } else if (std::holds_alternative<std::string>(value)) {
        const auto& text = std::get<std::string>(value);
        const auto& options = it->second.options;
        auto optionIt = std::find(options.begin(), options.end(), text);
        if (optionIt != options.end()) {
            controlValue = it->second.minimum + static_cast<int64_t>(std::distance(options.begin(), optionIt));
        } else {
            try {
                controlValue = std::stoll(text);
            } catch (...) {
                return false;
            }
        }
    }

    if (!V4L2ControlAccess::write(it->second, controlValue)) {
        LOG_WARNING("Could not apply V4L2 control '" + name + "' while " + (m_device.isStreaming() ? "streaming" : "stopped"));
        return false;
    }

    return true;
}

void V4L2Source::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    stopCapture();
    closeDevice();
}

void V4L2Source::stop()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    stopCapture();
}

ParameterSet V4L2Source::currentParameters() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    refreshCurrentParameterValues();

    ParameterSet parameters = configuredParameters();
    parameters.set("width", static_cast<int64_t>(m_width));
    parameters.set("height", static_cast<int64_t>(m_height));
    parameters.set("pixelformat", pixelFormatToString(m_pixelFormat));

    for (const auto& item : m_controlByParameter) {
        int64_t currentValue = 0;
        if (!V4L2ControlAccess::read(item.second, currentValue)) {
            continue;
        }

        ParameterValue value;
        if (item.second.type == V4L2_CTRL_TYPE_BOOLEAN) {
            value = currentValue != 0;
        } else if (item.second.type == V4L2_CTRL_TYPE_MENU || item.second.type == V4L2_CTRL_TYPE_INTEGER_MENU) {
            if (!item.second.options.empty() && currentValue >= 0 && currentValue < static_cast<int64_t>(item.second.options.size())) {
                value = item.second.options[static_cast<size_t>(currentValue)];
            } else {
                value = currentValue;
            }
        } else {
            value = currentValue;
        }
        parameters.set(item.first, value);
    }

    return parameters;
}

bool V4L2Source::generateTestFrame(FrameContext& context)
{
    ImageBuffer image;
    const uint32_t width = static_cast<uint32_t>(parameterInt("width", static_cast<int64_t>(m_width)));
    const uint32_t height = static_cast<uint32_t>(parameterInt("height", static_cast<int64_t>(m_height)));
    const PixelFormat format = pixelFormatFromString(parameterString("pixelformat", pixelFormatToString(m_pixelFormat)));
    const uint32_t stride = width * 2;
    image.allocate(width, height, stride, format);
    for (size_t i = 0; i < image.size(); ++i) {
        image.data()[i] = static_cast<uint8_t>((i + m_sequence) & 0xff);
    }

    auto now = std::chrono::steady_clock::now().time_since_epoch();
    image.setTimestampNs(static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count()));
    image.setSequence(++m_sequence);
    context.set("image", std::move(image));
    return true;
}

bool V4L2Source::openDevice()
{
    const std::string device = parameterString("device", "/dev/video0");
    const std::string subdevice = parameterString("subdevice", std::string());

    if (!m_device.open(device, O_RDWR | O_NONBLOCK)) {
        return false;
    }

    if (!m_device.supportsStreamingCapture()) {
        return false;
    }

    if (subdevice.empty()) {
        if (!m_subDevice.attachBorrowed(m_device.fd(), device)) {
            return false;
        }

    } else {
        if (!m_subDevice.open(subdevice, O_RDWR | O_NONBLOCK)) {
            return false;
        }
    }

    return true;
}

void V4L2Source::closeDevice()
{
    m_subDevice.close();
    m_device.close();
}

bool V4L2Source::startCapture()
{
    applyRequestedFormat();
    if (!m_device.startCapture(selectedFourcc(), m_bufferCount)) {
        LOG_ERROR("V4L2 capture start failed");
        return false;
    }
    return true;
}

void V4L2Source::stopCapture()
{
    m_device.stopCapture();
}

bool V4L2Source::refreshControlSchema()
{
    refreshDeviceOptions();
    const std::string device = parameterString("device", "/dev/video0");
    const std::string subdevice = parameterString("subdevice", std::string());

    V4L2Device temporaryDevice;
    V4L2Device temporarySubDevice;
    int deviceFd = m_device.fd();
    int subDeviceFd = m_subDevice.fd();

    if (deviceFd < 0) {
        if (!temporaryDevice.open(device, O_RDWR | O_NONBLOCK)) {
            m_controls.clear();
            m_controlSchema.clear();
            m_controlByParameter.clear();
            return true;
        }
        deviceFd = temporaryDevice.fd();
    }

    if (subdevice.empty()) {
        subDeviceFd = deviceFd;
    } else if (subDeviceFd < 0) {
        if (temporarySubDevice.open(subdevice, O_RDWR | O_NONBLOCK)) {
            subDeviceFd = temporarySubDevice.fd();
        } else {
            subDeviceFd = deviceFd;
        }
    }

    m_controls = V4L2ControlAccess::enumerate(deviceFd, m_device.name().empty() ? device : m_device.name(), subDeviceFd, m_subDevice.name().empty() ? subdevice : m_subDevice.name());

    m_controlSchema.clear();
    m_controlByParameter.clear();

    for (auto& control : m_controls) {
        int64_t currentValue = 0;
        ParameterInfo info = V4L2ControlAccess::toParameterInfo(control);
        if (V4L2ControlAccess::read(control, currentValue)) {
            if (info.type == ParameterType::Bool) {
                info.defaultValue = currentValue != 0;
            } else if (info.type != ParameterType::Option) {
                info.defaultValue = currentValue;
            }
        }
        m_controlSchema.push_back(info);
        m_controlByParameter[control.parameterName] = control;
    }
    return true;
}

void V4L2Source::refreshFormatOptions()
{
    m_formatOptions.clear();
    m_formatOptionLabels.clear();

    const std::string device = parameterString("device", "/dev/video0");
    V4L2Device temporaryDevice;
    if (!temporaryDevice.open(device, O_RDWR | O_NONBLOCK)) {
        return;
    }

    for (const auto& format : temporaryDevice.enumerateSupportedFormats()) {
        m_formatOptions.push_back(format.fourccName);
        if (format.description.empty()) {
            m_formatOptionLabels.push_back(format.fourccName);
        } else {
            m_formatOptionLabels.push_back(format.fourccName + " (" + format.description + ")");
        }
    }
}

bool V4L2Source::applyConfiguredControls()
{
    for (const auto& control : m_controlByParameter) {
        if (!isExplicitParameter(control.first)) {
            continue;
        }
        const ParameterValue* configuredValue = configuredParameters().value(control.first);
        if (configuredValue == nullptr) {
            continue;
        }
        applyControlParameterLocked(control.first, *configuredValue);
    }
    return true;
}

bool V4L2Source::isExplicitParameter(const std::string& name) const
{
    return m_explicitParameters.find(name) != m_explicitParameters.end();
}

bool V4L2Source::applyRequestedFormat()
{
    if (!m_device.isOpen()) {
        return false;
    }

    v4l2_format format{};
    if (!m_device.getFormat(format)) {
        return false;
    }

    const uint32_t requestedWidth = static_cast<uint32_t>(std::max<int64_t>(1, parameterInt("width", static_cast<int64_t>(m_width))));
    const uint32_t requestedHeight = static_cast<uint32_t>(std::max<int64_t>(1, parameterInt("height", static_cast<int64_t>(m_height))));
    const uint32_t requestedFourcc = selectedFourcc();

    switch (format.type) {
    case V4L2_BUF_TYPE_VIDEO_CAPTURE:
        if (isExplicitParameter("width")) {
            format.fmt.pix.width = requestedWidth;
        }
        if (isExplicitParameter("height")) {
            format.fmt.pix.height = requestedHeight;
        }
        if (isExplicitParameter("pixelformat") && requestedFourcc != 0) {
            format.fmt.pix.pixelformat = requestedFourcc;
        }
        break;
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
        if (isExplicitParameter("width")) {
            format.fmt.pix_mp.width = requestedWidth;
        }
        if (isExplicitParameter("height")) {
            format.fmt.pix_mp.height = requestedHeight;
        }
        if (isExplicitParameter("pixelformat") && requestedFourcc != 0) {
            format.fmt.pix_mp.pixelformat = requestedFourcc;
        }
        break;
    default:
        return false;
    }

    if (!m_device.setFormat(format)) {
        LOG_WARNING("Could not apply requested V4L2 format (" + std::to_string(requestedWidth) + "x" + std::to_string(requestedHeight) + ", " +
                    parameterString("pixelformat", pixelFormatToString(m_pixelFormat)) + ")");
        return false;
    }
    return true;
}

void V4L2Source::refreshDeviceOptions()
{
    m_deviceOptions = listDeviceEntriesWithPrefix("video");
    if (m_deviceOptions.empty()) {
        m_deviceOptions.push_back("/dev/video0");
    }

    m_deviceOptionLabels.clear();
    for (const auto& device : m_deviceOptions) {
        V4L2Device temporaryDevice;
        if (!temporaryDevice.open(device, O_RDWR | O_NONBLOCK)) {
            m_deviceOptionLabels.push_back(device);
            continue;
        }
        v4l2_capability capability{};
        if (!temporaryDevice.queryCapability(capability)) {
            m_deviceOptionLabels.push_back(device);
            continue;
        }
        m_deviceOptionLabels.push_back(device + " (" + reinterpret_cast<const char*>(capability.driver) + ", " + driverVersionString(capability.version) + ")");
    }

    m_subDeviceOptions = {""};
    m_subDeviceOptionLabels = {"(same as device)"};
    auto subdevices = listDeviceEntriesWithPrefix("v4l-subdev");
    m_subDeviceOptions.insert(m_subDeviceOptions.end(), subdevices.begin(), subdevices.end());
    for (const auto& device : subdevices) {
        V4L2Device temporaryDevice;
        if (!temporaryDevice.open(device, O_RDWR | O_NONBLOCK)) {
            m_subDeviceOptionLabels.push_back(device);
            continue;
        }
        v4l2_capability capability{};
        if (!temporaryDevice.queryCapability(capability)) {
            m_subDeviceOptionLabels.push_back(device);
            continue;
        }
        m_subDeviceOptionLabels.push_back(device + " (" + reinterpret_cast<const char*>(capability.driver) + ", " + driverVersionString(capability.version) + ")");
    }
}

void V4L2Source::refreshCurrentParameterValues() const
{
    if (!m_device.isOpen()) {
        return;
    }

    v4l2_format format{};
    if (m_device.getFormat(format)) {
        switch (format.type) {
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
            m_width = format.fmt.pix.width;
            m_height = format.fmt.pix.height;
            m_stride = format.fmt.pix.bytesperline;
            m_pixelFormat = pixelFormatFromFourCC(format.fmt.pix.pixelformat, m_pixelFormat);
            break;
        case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
            m_width = format.fmt.pix_mp.width;
            m_height = format.fmt.pix_mp.height;
            m_stride = format.fmt.pix_mp.plane_fmt[0].bytesperline;
            m_pixelFormat = pixelFormatFromFourCC(format.fmt.pix_mp.pixelformat, m_pixelFormat);
            break;
        default:
            break;
        }
    }

    for (auto& control : m_controlSchema) {
        auto it = m_controlByParameter.find(control.name);
        if (it == m_controlByParameter.end()) {
            continue;
        }

        int64_t currentValue = 0;
        if (!V4L2ControlAccess::read(it->second, currentValue)) {
            continue;
        }

        if (control.type == ParameterType::Bool) {
            control.defaultValue = currentValue != 0;
        } else if (control.type == ParameterType::Option) {
            if (!it->second.options.empty() && currentValue >= 0 && currentValue < static_cast<int64_t>(it->second.options.size())) {
                control.defaultValue = it->second.options[static_cast<size_t>(currentValue)];
            } else {
                control.defaultValue = currentValue;
            }
        } else {
            control.defaultValue = currentValue;
        }
    }
}

uint32_t V4L2Source::selectedFourcc() const
{
    return fourccFromString(parameterString("pixelformat", pixelFormatToString(m_pixelFormat)));
}

void V4L2Source::updateImageGeometry()
{
    v4l2_format format{};
    if (m_device.getFormat(format)) {
        switch (format.type) {
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
            m_width = format.fmt.pix.width;
            m_height = format.fmt.pix.height;
            m_stride = format.fmt.pix.bytesperline;
            m_pixelFormat = pixelFormatFromFourCC(format.fmt.pix.pixelformat, m_pixelFormat);
            break;
        case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
            m_width = format.fmt.pix_mp.width;
            m_height = format.fmt.pix_mp.height;
            m_stride = format.fmt.pix_mp.plane_fmt[0].bytesperline;
            m_pixelFormat = pixelFormatFromFourCC(format.fmt.pix_mp.pixelformat, m_pixelFormat);
            break;
        default:
            break;
        }
    }
    LOG_INFO("V4L2 format (width=" + std::to_string(m_width) + ", height=" + std::to_string(m_height) + ", stride=" + std::to_string(m_stride) + ", format=" + pixelFormatToString(m_pixelFormat) +
             ")");
}
