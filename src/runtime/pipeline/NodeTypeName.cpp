// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "NodeTypeName.hpp"

#include <algorithm>
#include <cctype>

static void replaceAll(std::string& text, const std::string& from, const std::string& to)
{
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
}

std::string normalizeNodeTypeName(const std::string& typeName)
{
    std::string result;
    result.reserve(typeName.size());
    for (char c : typeName) {
        if (c == '-' || c == '_' || c == ' ') {
            continue;
        }
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    replaceAll(result, "source", "src");
    return result;
}

std::string makeAutomaticNodeId(const std::string& typeName, size_t index)
{
    return normalizeNodeTypeName(typeName) + std::to_string(index);
}
