// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "V4L2Control.hpp"

#include "core/Logger.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstring>
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
    query.id = V4L2_CTRL_FLAG_NEXT_CTRL;

    while (::ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &query) == 0) {
        if ((query.flags & V4L2_CTRL_FLAG_DISABLED) == 0 && query.type != V4L2_CTRL_TYPE_CTRL_CLASS) {
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
            control.writable = (query.flags & (V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_GRABBED | V4L2_CTRL_FLAG_INACTIVE)) == 0;
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
                    }
                }
            }

            controls.push_back(control);
        }
        query.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }
}

bool V4L2ControlAccess::read(const V4L2Control& control, int64_t& value)
{
    if (control.fd < 0) {
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

bool V4L2ControlAccess::read(const V4L2Control& control, std::string& value)
{
    if (control.fd < 0) {
        return false;
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
    if ((control.flags & V4L2_CTRL_FLAG_GRABBED) != 0) {
        return reject("is currently grabbed by the driver");
    }
    if ((control.flags & V4L2_CTRL_FLAG_INACTIVE) != 0) {
        return reject("is currently inactive");
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
    if ((control.flags & V4L2_CTRL_FLAG_GRABBED) != 0) {
        return reject("is currently grabbed by the driver");
    }
    if ((control.flags & V4L2_CTRL_FLAG_INACTIVE) != 0) {
        return reject("is currently inactive");
    }
    if (!control.writable) {
        return reject("is currently not writable");
    }

    if (errorMessage != nullptr) {
        errorMessage->clear();
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
    if (control.type == V4L2_CTRL_TYPE_STRING) {
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
        defaultValue = control.options.front();
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
