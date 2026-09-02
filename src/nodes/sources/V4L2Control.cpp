// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "V4L2Control.hpp"

#include "core/Logger.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <sstream>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

std::string V4L2ControlAccess::parameterNameFromControlName(const std::string& name)
{
    std::string result;
    bool upperNext = false;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) == 0) {
            upperNext = !result.empty();
            continue;
        }
        if (result.empty()) {
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        } else if (upperNext) {
            result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            upperNext = false;
        } else {
            result.push_back(c);
        }
    }
    if (result.empty()) {
        result = "control";
    }
    return result;
}

std::vector<V4L2Control> V4L2ControlAccess::enumerate(int deviceFd, const std::string& deviceName, const std::vector<std::pair<int, std::string>>& subDevices)
{
    std::vector<V4L2Control> controls;
    std::map<std::string, int> names;
    enumerateFd(deviceFd, deviceName, controls, names);

    for (const auto& subDevice : subDevices) {
        if (subDevice.first < 0 || subDevice.first == deviceFd) {
            continue;
        }
        enumerateFd(subDevice.first, subDevice.second, controls, names);
    }

    return controls;
}

void V4L2ControlAccess::enumerateFd(int fd, const std::string& sourceDevice, std::vector<V4L2Control>& controls, std::map<std::string, int>& names)
{
    if (fd < 0) {
        return;
    }

    v4l2_query_ext_ctrl query;
    std::memset(&query, 0, sizeof(query));
    query.id = V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;

    while (::ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &query) == 0) {
        // Compound controls (u8/u16/u32 arrays, elems > 1) require the p_u8/p_u16/p_u32 payload
        // pointer in v4l2_ext_control rather than the scalar .value/.value64 fields; they are
        // exposed as a comma-separated string parameter (see read/write(std::string&) overloads).
        // Only U8/U16/U32 element types are supported this way; anything else (e.g. compound
        // struct controls) can't be represented generically and is skipped.
        const bool isCompoundControl = query.elems > 1 && query.type != V4L2_CTRL_TYPE_STRING;
        const bool isUnsupportedCompound = isCompoundControl && compoundElementSize(query.type) == 0;
        if ((query.flags & V4L2_CTRL_FLAG_DISABLED) == 0 && query.type != V4L2_CTRL_TYPE_CTRL_CLASS && isUnsupportedCompound) {
            LOG_WARNING("Skipping unsupported compound V4L2 control '" + std::string(reinterpret_cast<const char*>(query.name)) + "' (" + std::to_string(query.elems) + " elements) on " +
                        sourceDevice);
        }
        if ((query.flags & V4L2_CTRL_FLAG_DISABLED) == 0 && query.type != V4L2_CTRL_TYPE_CTRL_CLASS && !isUnsupportedCompound) {
            V4L2Control control;
            control.controlName = reinterpret_cast<const char*>(query.name);
            control.parameterName = parameterNameFromControlName(control.controlName);
            int count = ++names[control.parameterName];
            if (count > 1) {
                control.parameterName += std::to_string(count);
            }
            control.id = query.id;
            control.type = query.type;
            control.minimum = query.minimum;
            control.maximum = query.maximum;
            control.step = query.step;
            control.defaultValue = query.default_value;
            control.fd = fd;
            control.flags = query.flags;
            control.elems = query.elems == 0 ? 1 : query.elems;
            control.readable = (query.flags & V4L2_CTRL_FLAG_WRITE_ONLY) == 0;
            control.writable = (query.flags & V4L2_CTRL_FLAG_READ_ONLY) == 0;
            control.runtimeWritable = control.writable && (query.flags & V4L2_CTRL_FLAG_MODIFY_LAYOUT) == 0;
            control.sourceDevice = sourceDevice;

            if (query.type == V4L2_CTRL_TYPE_MENU || query.type == V4L2_CTRL_TYPE_INTEGER_MENU) {
                for (int64_t index = query.minimum; index <= query.maximum; ++index) {
                    v4l2_querymenu menu;
                    std::memset(&menu, 0, sizeof(menu));
                    menu.id = query.id;
                    menu.index = static_cast<uint32_t>(index);
                    if (::ioctl(fd, VIDIOC_QUERYMENU, &menu) == 0) {
                        if (query.type == V4L2_CTRL_TYPE_MENU) {
                            control.options.emplace_back(reinterpret_cast<const char*>(menu.name));
                        } else {
                            control.options.emplace_back(std::to_string(menu.value));
                        }
                        control.optionValues.push_back(index);
                    }
                }
            }

            controls.push_back(control);
        }
        query.id |= V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;
    }
}

bool V4L2ControlAccess::read(const V4L2Control& control, int64_t& value)
{
    if (control.fd < 0 || !control.readable) {
        return false;
    }

    v4l2_ext_control extControl;
    std::memset(&extControl, 0, sizeof(extControl));
    extControl.id = control.id;
    v4l2_ext_controls extControls;
    std::memset(&extControls, 0, sizeof(extControls));
    extControls.count = 1;
    extControls.controls = &extControl;
    if (::ioctl(control.fd, VIDIOC_G_EXT_CTRLS, &extControls) == 0) {
        value = control.type == V4L2_CTRL_TYPE_INTEGER64 ? extControl.value64 : extControl.value;
        return true;
    }

    v4l2_control simpleControl;
    std::memset(&simpleControl, 0, sizeof(simpleControl));
    simpleControl.id = control.id;
    if (::ioctl(control.fd, VIDIOC_G_CTRL, &simpleControl) == 0) {
        value = simpleControl.value;
        return true;
    }
    value = 0;
    return false;
}

size_t V4L2ControlAccess::compoundElementSize(uint32_t type)
{
    switch (type) {
    case V4L2_CTRL_TYPE_U8:
        return 1;
    case V4L2_CTRL_TYPE_U16:
        return 2;
    case V4L2_CTRL_TYPE_U32:
        return 4;
    default:
        return 0;
    }
}

namespace
{

void assignCompoundPointer(v4l2_ext_control& extControl, uint32_t type, uint8_t* data)
{
    switch (type) {
    case V4L2_CTRL_TYPE_U8:
        extControl.p_u8 = data;
        break;
    case V4L2_CTRL_TYPE_U16:
        extControl.p_u16 = reinterpret_cast<uint16_t*>(data);
        break;
    case V4L2_CTRL_TYPE_U32:
        extControl.p_u32 = reinterpret_cast<uint32_t*>(data);
        break;
    default:
        break;
    }
}

uint64_t readCompoundElement(uint32_t type, const uint8_t* data, uint32_t index)
{
    switch (type) {
    case V4L2_CTRL_TYPE_U8:
        return data[index];
    case V4L2_CTRL_TYPE_U16:
        return reinterpret_cast<const uint16_t*>(data)[index];
    case V4L2_CTRL_TYPE_U32:
        return reinterpret_cast<const uint32_t*>(data)[index];
    default:
        return 0;
    }
}

void writeCompoundElement(uint32_t type, uint8_t* data, uint32_t index, uint64_t value)
{
    switch (type) {
    case V4L2_CTRL_TYPE_U8:
        data[index] = static_cast<uint8_t>(value);
        break;
    case V4L2_CTRL_TYPE_U16:
        reinterpret_cast<uint16_t*>(data)[index] = static_cast<uint16_t>(value);
        break;
    case V4L2_CTRL_TYPE_U32:
        reinterpret_cast<uint32_t*>(data)[index] = static_cast<uint32_t>(value);
        break;
    default:
        break;
    }
}

} // namespace

bool V4L2ControlAccess::read(const V4L2Control& control, std::string& value)
{
    if (control.fd < 0 || !control.readable) {
        return false;
    }

    if (control.elems > 1 && control.type != V4L2_CTRL_TYPE_STRING) {
        const size_t elementSize = V4L2ControlAccess::compoundElementSize(control.type);
        if (elementSize == 0) {
            return false;
        }
        std::vector<uint8_t> buffer(static_cast<size_t>(control.elems) * elementSize, 0);

        v4l2_ext_control extControl;
        std::memset(&extControl, 0, sizeof(extControl));
        extControl.id = control.id;
        extControl.size = static_cast<unsigned int>(buffer.size());
        assignCompoundPointer(extControl, control.type, buffer.data());

        v4l2_ext_controls extControls;
        std::memset(&extControls, 0, sizeof(extControls));
        extControls.count = 1;
        extControls.controls = &extControl;
        if (::ioctl(control.fd, VIDIOC_G_EXT_CTRLS, &extControls) != 0) {
            value.clear();
            return false;
        }

        value.clear();
        for (uint32_t index = 0; index < control.elems; ++index) {
            if (index > 0) {
                value += ",";
            }
            value += std::to_string(readCompoundElement(control.type, buffer.data(), index));
        }
        return true;
    }

    const size_t bufferSize = std::max<size_t>(64u, static_cast<size_t>(std::max<int64_t>(control.maximum + 1, 1)));
    std::vector<char> buffer(bufferSize, '\0');

    v4l2_ext_control extControl;
    std::memset(&extControl, 0, sizeof(extControl));
    extControl.id = control.id;
    extControl.size = static_cast<unsigned int>(buffer.size());
    extControl.string = buffer.data();

    v4l2_ext_controls extControls;
    std::memset(&extControls, 0, sizeof(extControls));
    extControls.count = 1;
    extControls.controls = &extControl;
    if (::ioctl(control.fd, VIDIOC_G_EXT_CTRLS, &extControls) != 0) {
        value.clear();
        return false;
    }

    const size_t length = std::min<size_t>(buffer.size(), static_cast<size_t>(extControl.size));
    size_t end = 0;
    while (end < length && buffer[end] != '\0') {
        ++end;
    }
    value.assign(buffer.data(), end);
    return true;
}

bool V4L2ControlAccess::write(const V4L2Control& control, int64_t value, std::string* errorMessage)
{
    const auto reject = [&control, errorMessage](const std::string& reason) {
        if (errorMessage != nullptr) {
            *errorMessage = "V4L2 control '" + control.controlName + "' " + reason;
        }
        return false;
    };

    if (control.fd < 0) {
        return reject("has no open device");
    }
    if ((control.flags & V4L2_CTRL_FLAG_READ_ONLY) != 0) {
        return reject("is read-only");
    }
    if (!control.writable) {
        return reject("is currently not writable");
    }

    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    v4l2_ext_control extControl;
    std::memset(&extControl, 0, sizeof(extControl));
    extControl.id = control.id;
    if (control.type == V4L2_CTRL_TYPE_INTEGER64) {
        extControl.value64 = value;
    } else {
        extControl.value = static_cast<int32_t>(value);
    }

    v4l2_ext_controls extControls;
    std::memset(&extControls, 0, sizeof(extControls));
    extControls.count = 1;
    extControls.controls = &extControl;
    if (::ioctl(control.fd, VIDIOC_S_EXT_CTRLS, &extControls) == 0) {
        return true;
    }
    const int extendedError = errno;

    v4l2_control simpleControl;
    std::memset(&simpleControl, 0, sizeof(simpleControl));
    simpleControl.id = control.id;
    simpleControl.value = static_cast<int32_t>(value);
    if (::ioctl(control.fd, VIDIOC_S_CTRL, &simpleControl) == 0) {
        return true;
    }

    const int simpleError = errno;
    const int controlError = simpleError == ENOTTY || simpleError == EINVAL ? extendedError : simpleError;
    return reject("write failed: " + std::string(std::strerror(controlError)) + " (errno " + std::to_string(controlError) + ")");
}

bool V4L2ControlAccess::write(const V4L2Control& control, const std::string& value, std::string* errorMessage)
{
    const auto reject = [&control, errorMessage](const std::string& reason) {
        if (errorMessage != nullptr) {
            *errorMessage = "V4L2 control '" + control.controlName + "' " + reason;
        }
        return false;
    };

    if (control.fd < 0) {
        return reject("has no open device");
    }
    if ((control.flags & V4L2_CTRL_FLAG_READ_ONLY) != 0) {
        return reject("is read-only");
    }
    if (!control.writable) {
        return reject("is currently not writable");
    }

    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    if (control.elems > 1 && control.type != V4L2_CTRL_TYPE_STRING) {
        const size_t elementSize = V4L2ControlAccess::compoundElementSize(control.type);
        if (elementSize == 0) {
            return reject("has an unsupported compound element type");
        }

        std::vector<uint64_t> parsedValues;
        std::stringstream stream(value);
        std::string token;
        while (std::getline(stream, token, ',')) {
            try {
                parsedValues.push_back(std::stoull(token));
            } catch (...) {
                return reject("has an invalid array value '" + value + "'");
            }
        }
        if (parsedValues.size() != control.elems) {
            return reject("expects " + std::to_string(control.elems) + " comma-separated values, got " + std::to_string(parsedValues.size()));
        }

        std::vector<uint8_t> buffer(static_cast<size_t>(control.elems) * elementSize, 0);
        for (uint32_t index = 0; index < control.elems; ++index) {
            writeCompoundElement(control.type, buffer.data(), index, parsedValues[index]);
        }

        v4l2_ext_control extControl;
        std::memset(&extControl, 0, sizeof(extControl));
        extControl.id = control.id;
        extControl.size = static_cast<unsigned int>(buffer.size());
        assignCompoundPointer(extControl, control.type, buffer.data());

        v4l2_ext_controls extControls;
        std::memset(&extControls, 0, sizeof(extControls));
        extControls.count = 1;
        extControls.controls = &extControl;
        if (::ioctl(control.fd, VIDIOC_S_EXT_CTRLS, &extControls) == 0) {
            return true;
        }
        return reject("write failed: " + std::string(std::strerror(errno)) + " (errno " + std::to_string(errno) + ")");
    }

    const size_t bufferSize = std::max<size_t>(64u, value.size() + 1u);
    std::vector<char> buffer(bufferSize, '\0');
    std::memcpy(buffer.data(), value.c_str(), value.size());

    v4l2_ext_control extControl;
    std::memset(&extControl, 0, sizeof(extControl));
    extControl.id = control.id;
    extControl.size = static_cast<unsigned int>(buffer.size());
    extControl.string = buffer.data();

    v4l2_ext_controls extControls;
    std::memset(&extControls, 0, sizeof(extControls));
    extControls.count = 1;
    extControls.controls = &extControl;
    if (::ioctl(control.fd, VIDIOC_S_EXT_CTRLS, &extControls) == 0) {
        return true;
    }

    return reject("write failed: " + std::string(std::strerror(errno)) + " (errno " + std::to_string(errno) + ")");
}

ParameterInfo V4L2ControlAccess::toParameterInfo(const V4L2Control& control)
{
    ParameterType type = ParameterType::Int;
    if (control.type == V4L2_CTRL_TYPE_BOOLEAN) {
        type = ParameterType::Bool;
    }
    if (control.type == V4L2_CTRL_TYPE_BUTTON) {
        type = ParameterType::Button;
    }
    if (control.type == V4L2_CTRL_TYPE_MENU || control.type == V4L2_CTRL_TYPE_INTEGER_MENU) {
        type = ParameterType::Option;
    }
    if (control.type == V4L2_CTRL_TYPE_STRING || (control.elems > 1 && control.type != V4L2_CTRL_TYPE_STRING)) {
        type = ParameterType::String;
    }

    ParameterValue defaultValue = control.defaultValue;
    ParameterValue minimumValue = control.minimum;
    ParameterValue maximumValue = control.maximum;
    if (type == ParameterType::Bool) {
        defaultValue = control.defaultValue != 0;
        minimumValue = false;
        maximumValue = true;
    }
    if (type == ParameterType::Option && !control.options.empty()) {
        const auto valueIt = std::find(control.optionValues.begin(), control.optionValues.end(), control.defaultValue);
        defaultValue = valueIt == control.optionValues.end() ? control.options.front() : control.options[static_cast<size_t>(std::distance(control.optionValues.begin(), valueIt))];
        minimumValue = std::string();
        maximumValue = std::string();
    }
    if (type == ParameterType::Button) {
        defaultValue = int64_t(0);
        minimumValue = int64_t(0);
        maximumValue = int64_t(1);
    }
    if (type == ParameterType::String) {
        defaultValue = std::string();
        minimumValue = std::string();
        maximumValue = std::string();
    }

    ParameterInfo info{
        control.parameterName, type, "V4L2 control: " + control.controlName, defaultValue, minimumValue, maximumValue, control.options, control.runtimeWritable, control.options, std::string("v4l2"),
        control.sourceDevice};
    info.readOnly = !control.writable;
    return info;
}
