// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "MediaGraphInspector.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <linux/media-bus-format.h>
#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <linux/videodev2.h>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace
{

std::string jsonEscape(const std::string& value)
{
    std::string escaped;
    for (const char character : value) {
        switch (character) {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += character;
            break;
        }
    }
    return escaped;
}

std::vector<std::string> mediaDevices()
{
    std::vector<std::string> devices;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator("/dev", error)) {
        if (error) {
            break;
        }
        const std::string name = entry.path().filename().string();
        if (name.rfind("media", 0) == 0 && name.size() > 5 && std::all_of(name.begin() + 5, name.end(), [](char value) { return value >= '0' && value <= '9'; })) {
            devices.push_back(entry.path().string());
        }
    }
    std::sort(devices.begin(), devices.end());
    return devices;
}

bool isKnownMediaDevice(const std::string& device)
{
    const auto devices = mediaDevices();
    return std::find(devices.begin(), devices.end(), device) != devices.end();
}

std::unordered_map<uint64_t, std::string> devnodePaths()
{
    std::unordered_map<uint64_t, std::string> paths;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator("/dev", error)) {
        if (error) {
            break;
        }
        struct stat status
        {
        };
        const std::string path = entry.path().string();
        if (::stat(path.c_str(), &status) != 0 || !S_ISCHR(status.st_mode)) {
            continue;
        }
        const uint64_t key = (static_cast<uint64_t>(major(status.st_rdev)) << 32u) | minor(status.st_rdev);
        paths.emplace(key, path);
    }
    return paths;
}

std::string mediaBusFormatName(uint32_t code)
{
    switch (code) {
    case MEDIA_BUS_FMT_RGB888_1X24:
        return "RGB888_1X24";
    case MEDIA_BUS_FMT_UYVY8_2X8:
        return "UYVY8_2X8";
    case MEDIA_BUS_FMT_UYVY8_1X16:
        return "UYVY8_1X16";
    case MEDIA_BUS_FMT_YUYV8_2X8:
        return "YUYV8_2X8";
    case MEDIA_BUS_FMT_YUYV8_1X16:
        return "YUYV8_1X16";
    case MEDIA_BUS_FMT_SBGGR8_1X8:
        return "SBGGR8_1X8";
    case MEDIA_BUS_FMT_SGBRG8_1X8:
        return "SGBRG8_1X8";
    case MEDIA_BUS_FMT_SGRBG8_1X8:
        return "SGRBG8_1X8";
    case MEDIA_BUS_FMT_SRGGB8_1X8:
        return "SRGGB8_1X8";
    case MEDIA_BUS_FMT_SBGGR10_1X10:
        return "SBGGR10_1X10";
    case MEDIA_BUS_FMT_SGBRG10_1X10:
        return "SGBRG10_1X10";
    case MEDIA_BUS_FMT_SGRBG10_1X10:
        return "SGRBG10_1X10";
    case MEDIA_BUS_FMT_SRGGB10_1X10:
        return "SRGGB10_1X10";
    case MEDIA_BUS_FMT_SBGGR12_1X12:
        return "SBGGR12_1X12";
    case MEDIA_BUS_FMT_SGBRG12_1X12:
        return "SGBRG12_1X12";
    case MEDIA_BUS_FMT_SGRBG12_1X12:
        return "SGRBG12_1X12";
    case MEDIA_BUS_FMT_SRGGB12_1X12:
        return "SRGGB12_1X12";
    case MEDIA_BUS_FMT_SBGGR14_1X14:
        return "SBGGR14_1X14";
    case MEDIA_BUS_FMT_SGBRG14_1X14:
        return "SGBRG14_1X14";
    case MEDIA_BUS_FMT_SGRBG14_1X14:
        return "SGRBG14_1X14";
    case MEDIA_BUS_FMT_SRGGB14_1X14:
        return "SRGGB14_1X14";
    case MEDIA_BUS_FMT_SBGGR16_1X16:
        return "SBGGR16_1X16";
    case MEDIA_BUS_FMT_SGBRG16_1X16:
        return "SGBRG16_1X16";
    case MEDIA_BUS_FMT_SGRBG16_1X16:
        return "SGRBG16_1X16";
    case MEDIA_BUS_FMT_SRGGB16_1X16:
        return "SRGGB16_1X16";
    default: {
        std::ostringstream value;
        value << "0x" << std::hex << code;
        return value.str();
    }
    }
}

std::string fourccName(uint32_t code)
{
    std::string value(4, ' ');
    value[0] = static_cast<char>(code & 0xffu);
    value[1] = static_cast<char>((code >> 8u) & 0xffu);
    value[2] = static_cast<char>((code >> 16u) & 0xffu);
    value[3] = static_cast<char>((code >> 24u) & 0xffu);
    for (char& character : value) {
        if (character < 32 || character > 126) {
            character = '?';
        }
    }
    return value;
}

struct PadFormat
{
    std::string pixelFormat;
    uint32_t width = 0;
    uint32_t height = 0;
};

PadFormat readPadFormat(const std::string& devnode, uint32_t interfaceType, uint32_t padIndex)
{
    PadFormat result;
    const int fd = ::open(devnode.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        return result;
    }

    if (interfaceType == MEDIA_INTF_T_V4L_SUBDEV) {
        v4l2_subdev_format format{};
        format.which = V4L2_SUBDEV_FORMAT_ACTIVE;
        format.pad = padIndex;
        if (::ioctl(fd, VIDIOC_SUBDEV_G_FMT, &format) == 0) {
            result.pixelFormat = mediaBusFormatName(format.format.code);
            result.width = format.format.width;
            result.height = format.format.height;
        }
    } else if (interfaceType == MEDIA_INTF_T_V4L_VIDEO) {
        for (const uint32_t type : {V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_BUF_TYPE_VIDEO_OUTPUT, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE}) {
            v4l2_format format{};
            format.type = type;
            if (::ioctl(fd, VIDIOC_G_FMT, &format) == 0) {
                if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE || type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
                    result.pixelFormat = fourccName(format.fmt.pix_mp.pixelformat);
                    result.width = format.fmt.pix_mp.width;
                    result.height = format.fmt.pix_mp.height;
                } else {
                    result.pixelFormat = fourccName(format.fmt.pix.pixelformat);
                    result.width = format.fmt.pix.width;
                    result.height = format.fmt.pix.height;
                }
                break;
            }
        }
    }
    ::close(fd);
    return result;
}

std::string entityFunctionName(uint32_t function)
{
    switch (function) {
    case MEDIA_ENT_F_CAM_SENSOR:
        return "camera sensor";
    case MEDIA_ENT_F_LENS:
        return "lens";
    case MEDIA_ENT_F_FLASH:
        return "flash";
    case MEDIA_ENT_F_IO_V4L:
        return "V4L I/O";
    case MEDIA_ENT_F_PROC_VIDEO_SCALER:
        return "video scaler";
    case MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER:
        return "pixel formatter";
    case MEDIA_ENT_F_VID_IF_BRIDGE:
        return "video interface bridge";
    default: {
        std::ostringstream value;
        value << "0x" << std::hex << function;
        return value.str();
    }
    }
}

} // namespace

std::string MediaGraphInspector::devicesJson()
{
    const auto devices = mediaDevices();
    std::ostringstream json;
    json << "{\"devices\":[";
    for (size_t index = 0; index < devices.size(); ++index) {
        if (index) {
            json << ',';
        }
        json << "\"" << jsonEscape(devices[index]) << "\"";
    }
    json << "]}";
    return json.str();
}

bool MediaGraphInspector::graphJson(const std::string& device, std::string& jsonText, std::string& errorMessage)
{
    if (!isKnownMediaDevice(device)) {
        errorMessage = "media device not found";
        return false;
    }

    const int fd = ::open(device.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        errorMessage = std::strerror(errno);
        return false;
    }

    media_device_info deviceInfo{};
    if (::ioctl(fd, MEDIA_IOC_DEVICE_INFO, &deviceInfo) != 0) {
        errorMessage = std::strerror(errno);
        ::close(fd);
        return false;
    }

    media_v2_topology topology{};
    if (::ioctl(fd, MEDIA_IOC_G_TOPOLOGY, &topology) != 0) {
        errorMessage = std::strerror(errno);
        ::close(fd);
        return false;
    }

    std::vector<media_v2_entity> entities(topology.num_entities);
    std::vector<media_v2_interface> interfaces(topology.num_interfaces);
    std::vector<media_v2_pad> pads(topology.num_pads);
    std::vector<media_v2_link> links(topology.num_links);
    topology.ptr_entities = reinterpret_cast<uintptr_t>(entities.data());
    topology.ptr_interfaces = reinterpret_cast<uintptr_t>(interfaces.data());
    topology.ptr_pads = reinterpret_cast<uintptr_t>(pads.data());
    topology.ptr_links = reinterpret_cast<uintptr_t>(links.data());
    if (::ioctl(fd, MEDIA_IOC_G_TOPOLOGY, &topology) != 0) {
        errorMessage = std::strerror(errno);
        ::close(fd);
        return false;
    }
    ::close(fd);

    const auto paths = devnodePaths();
    std::unordered_map<uint32_t, const media_v2_interface*> interfaceById;
    for (const auto& interface : interfaces) {
        interfaceById.emplace(interface.id, &interface);
    }
    std::unordered_map<uint32_t, uint32_t> entityInterface;
    for (const auto& link : links) {
        if (interfaceById.count(link.source_id) != 0) {
            entityInterface[link.sink_id] = link.source_id;
        }
        if (interfaceById.count(link.sink_id) != 0) {
            entityInterface[link.source_id] = link.sink_id;
        }
    }
    std::unordered_map<uint32_t, const media_v2_pad*> padById;
    for (const auto& pad : pads) {
        padById.emplace(pad.id, &pad);
    }

    std::ostringstream output;
    output << "{\"device\":\"" << jsonEscape(device) << "\",\"driver\":\"" << jsonEscape(reinterpret_cast<const char*>(deviceInfo.driver)) << "\",\"model\":\""
           << jsonEscape(reinterpret_cast<const char*>(deviceInfo.model)) << "\",\"serial\":\"" << jsonEscape(reinterpret_cast<const char*>(deviceInfo.serial)) << "\",\"entities\":[";

    for (size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex) {
        const auto& entity = entities[entityIndex];
        if (entityIndex) {
            output << ',';
        }
        std::string devnode;
        uint32_t interfaceType = 0;
        const auto entityInterfaceIt = entityInterface.find(entity.id);
        if (entityInterfaceIt != entityInterface.end()) {
            const auto interfaceIt = interfaceById.find(entityInterfaceIt->second);
            if (interfaceIt != interfaceById.end()) {
                interfaceType = interfaceIt->second->intf_type;
                const uint64_t key = (static_cast<uint64_t>(interfaceIt->second->devnode.major) << 32u) | interfaceIt->second->devnode.minor;
                const auto pathIt = paths.find(key);
                if (pathIt != paths.end()) {
                    devnode = pathIt->second;
                }
            }
        }
        output << "{\"id\":" << entity.id << ",\"name\":\"" << jsonEscape(entity.name) << "\",\"function\":\"" << jsonEscape(entityFunctionName(entity.function))
               << "\",\"functionId\":" << entity.function << ",\"flags\":" << entity.flags << ",\"devnode\":\"" << jsonEscape(devnode) << "\",\"pads\":[";
        bool firstPad = true;
        for (const auto& pad : pads) {
            if (pad.entity_id != entity.id) {
                continue;
            }
            if (!firstPad) {
                output << ',';
            }
            firstPad = false;
            const PadFormat format = readPadFormat(devnode, interfaceType, pad.index);
            output << "{\"id\":" << pad.id << ",\"index\":" << pad.index << ",\"flags\":" << pad.flags << ",\"pixelFormat\":\"" << jsonEscape(format.pixelFormat) << "\",\"width\":" << format.width
                   << ",\"height\":" << format.height << "}";
        }
        output << "]}";
    }

    output << "],\"links\":[";
    bool firstLink = true;
    for (const auto& link : links) {
        const auto source = padById.find(link.source_id);
        const auto sink = padById.find(link.sink_id);
        if (source == padById.end() || sink == padById.end()) {
            continue;
        }
        if (!firstLink) {
            output << ',';
        }
        firstLink = false;
        output << "{\"id\":" << link.id << ",\"sourceEntityId\":" << source->second->entity_id << ",\"sourcePadId\":" << link.source_id << ",\"sinkEntityId\":" << sink->second->entity_id
               << ",\"sinkPadId\":" << link.sink_id << ",\"flags\":" << link.flags << "}";
    }
    output << "]}";
    jsonText = output.str();
    return true;
}
