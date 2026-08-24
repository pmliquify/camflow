// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "DeviceTreeInspector.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

// Node counts are bounded so a malformed or cyclic tree can never exhaust memory.
constexpr size_t kMaxPropertyBytes = 4096;
constexpr size_t kMaxDepth = 32;
constexpr size_t kMaxNodes = 20000;

const std::array<const char*, 2> kDeviceTreeRoots = {"/sys/firmware/devicetree/base", "/proc/device-tree"};

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

std::string hexBytes(const std::vector<uint8_t>& data)
{
    static const char digits[] = "0123456789abcdef";
    std::string text;
    text.reserve(data.size() * 2);
    for (const uint8_t value : data) {
        text += digits[value >> 4];
        text += digits[value & 0x0f];
    }
    return text;
}

bool isStringValue(const std::vector<uint8_t>& data)
{
    if (data.empty() || data.back() != 0) {
        return false;
    }
    if (data.size() == 1) {
        return true;
    }
    bool segmentHasCharacters = false;
    for (size_t index = 0; index + 1 < data.size(); ++index) {
        const uint8_t value = data[index];
        if (value == 0) {
            if (!segmentHasCharacters) {
                return false;
            }
            segmentHasCharacters = false;
            continue;
        }
        if (value < 0x20 || value > 0x7e) {
            return false;
        }
        segmentHasCharacters = true;
    }
    return segmentHasCharacters;
}

std::vector<std::string> splitStrings(const std::vector<uint8_t>& data)
{
    std::vector<std::string> strings;
    std::string current;
    for (size_t index = 0; index < data.size(); ++index) {
        if (data[index] == 0) {
            strings.push_back(current);
            current.clear();
            continue;
        }
        current += static_cast<char>(data[index]);
    }
    return strings;
}

bool readPropertyBytes(const std::filesystem::path& path, std::vector<uint8_t>& data, bool& truncated)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return false;
    }
    data.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    truncated = data.size() > kMaxPropertyBytes;
    if (truncated) {
        data.resize(kMaxPropertyBytes);
    }
    return true;
}

void appendProperty(std::ostringstream& output, const std::string& name, const std::filesystem::path& path)
{
    std::vector<uint8_t> data;
    bool truncated = false;
    const bool readable = readPropertyBytes(path, data, truncated);

    output << "{\"name\":\"" << jsonEscape(name) << "\",\"length\":" << data.size() << ",\"truncated\":" << (truncated ? "true" : "false");
    if (!readable) {
        output << ",\"type\":\"unreadable\"}";
        return;
    }
    if (data.empty()) {
        output << ",\"type\":\"empty\"}";
        return;
    }
    if (isStringValue(data)) {
        const auto strings = splitStrings(data);
        output << ",\"type\":\"" << (strings.size() > 1 ? "stringList" : "string") << "\",\"strings\":[";
        for (size_t index = 0; index < strings.size(); ++index) {
            if (index) {
                output << ',';
            }
            output << "\"" << jsonEscape(strings[index]) << "\"";
        }
        output << "]}";
        return;
    }
    if (!truncated && data.size() % 4 == 0) {
        output << ",\"type\":\"cells\",\"cells\":[";
        for (size_t index = 0; index < data.size(); index += 4) {
            if (index) {
                output << ',';
            }
            const uint32_t cell =
                (static_cast<uint32_t>(data[index]) << 24) | (static_cast<uint32_t>(data[index + 1]) << 16) | (static_cast<uint32_t>(data[index + 2]) << 8) | static_cast<uint32_t>(data[index + 3]);
            output << cell;
        }
        output << "]}";
        return;
    }
    output << ",\"type\":\"bytes\",\"bytes\":\"" << hexBytes(data) << "\"}";
}

void appendNode(std::ostringstream& output, const std::filesystem::path& path, const std::string& name, const std::string& nodePath, size_t depth, size_t& nodeCount)
{
    ++nodeCount;
    output << "{\"name\":\"" << jsonEscape(name) << "\",\"path\":\"" << jsonEscape(nodePath) << "\"";

    std::vector<std::filesystem::path> propertyPaths;
    std::vector<std::filesystem::path> childPaths;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(path, std::filesystem::directory_options::skip_permission_denied, error)) {
        if (error) {
            break;
        }
        // Device tree symlinks (alias shortcuts) would duplicate whole subtrees.
        if (entry.is_symlink()) {
            continue;
        }
        if (entry.is_directory()) {
            childPaths.push_back(entry.path());
        } else if (entry.is_regular_file()) {
            propertyPaths.push_back(entry.path());
        }
    }
    std::sort(propertyPaths.begin(), propertyPaths.end());
    std::sort(childPaths.begin(), childPaths.end());

    output << ",\"properties\":[";
    for (size_t index = 0; index < propertyPaths.size(); ++index) {
        if (index) {
            output << ',';
        }
        appendProperty(output, propertyPaths[index].filename().string(), propertyPaths[index]);
    }
    output << "],\"children\":[";
    if (depth < kMaxDepth) {
        bool first = true;
        for (const auto& childPath : childPaths) {
            if (nodeCount >= kMaxNodes) {
                break;
            }
            if (!first) {
                output << ',';
            }
            first = false;
            const std::string childName = childPath.filename().string();
            appendNode(output, childPath, childName, nodePath == "/" ? "/" + childName : nodePath + "/" + childName, depth + 1, nodeCount);
        }
    }
    output << "]}";
}

} // namespace

bool DeviceTreeInspector::treeJson(std::string& jsonText, std::string& errorMessage)
{
    std::filesystem::path root;
    for (const char* candidate : kDeviceTreeRoots) {
        std::error_code error;
        if (std::filesystem::is_directory(candidate, error) && !error) {
            root = candidate;
            break;
        }
    }
    if (root.empty()) {
        errorMessage = "device tree not available";
        return false;
    }

    size_t nodeCount = 0;
    std::ostringstream output;
    output << "{\"root\":\"" << jsonEscape(root.string()) << "\",\"node\":";
    appendNode(output, root, "/", "/", 0, nodeCount);
    output << ",\"nodeCount\":" << nodeCount << ",\"truncated\":" << (nodeCount >= kMaxNodes ? "true" : "false") << "}";
    jsonText = output.str();
    return true;
}
