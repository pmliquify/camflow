// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "ModuleDebugInspector.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
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

std::string trim(const std::string& value)
{
    size_t start = 0;
    size_t end = value.size();
    while (start < end && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

std::string readFileTrimmed(const std::filesystem::path& path)
{
    std::ifstream stream(path);
    if (!stream) {
        return {};
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return trim(buffer.str());
}

std::vector<std::string> otherParameterNames(const std::filesystem::path& parametersDir)
{
    std::vector<std::string> names;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(parametersDir, std::filesystem::directory_options::skip_permission_denied, error)) {
        if (error) {
            break;
        }
        if (entry.is_regular_file() && entry.path().filename() != "debug") {
            names.push_back(entry.path().filename().string());
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool isValidModuleName(const std::string& module)
{
    return !module.empty() && module.find('/') == std::string::npos && module != "." && module != "..";
}

} // namespace

std::string ModuleDebugInspector::listJson()
{
    std::vector<std::filesystem::path> moduleDirs;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator("/sys/module", std::filesystem::directory_options::skip_permission_denied, error)) {
        if (error) {
            break;
        }
        if (entry.is_directory()) {
            moduleDirs.push_back(entry.path());
        }
    }
    std::sort(moduleDirs.begin(), moduleDirs.end());

    std::ostringstream output;
    output << "{\"modules\":[";
    bool first = true;
    for (const auto& moduleDir : moduleDirs) {
        const std::filesystem::path debugPath = moduleDir / "parameters" / "debug";
        std::error_code statusError;
        if (!std::filesystem::is_regular_file(debugPath, statusError) || statusError) {
            continue;
        }

        const std::string moduleName = moduleDir.filename().string();
        const std::string rawValue = readFileTrimmed(debugPath);
        const std::string version = readFileTrimmed(moduleDir / "version");
        const std::string refcnt = readFileTrimmed(moduleDir / "refcnt");
        const std::string initstate = readFileTrimmed(moduleDir / "initstate");
        std::error_code permissionError;
        const auto permissions = std::filesystem::status(debugPath, permissionError).permissions();
        const bool writable = !permissionError && (permissions & std::filesystem::perms::owner_write) != std::filesystem::perms::none;

        if (!first) {
            output << ',';
        }
        first = false;
        output << "{\"name\":\"" << jsonEscape(moduleName) << "\",\"path\":\"" << jsonEscape(debugPath.string()) << "\",\"value\":\"" << jsonEscape(rawValue)
               << "\",\"writable\":" << (writable ? "true" : "false") << ",\"version\":\"" << jsonEscape(version) << "\",\"refcnt\":\"" << jsonEscape(refcnt) << "\",\"initstate\":\""
               << jsonEscape(initstate) << "\",\"parameters\":[";
        const auto parameterNames = otherParameterNames(moduleDir / "parameters");
        for (size_t index = 0; index < parameterNames.size(); ++index) {
            if (index) {
                output << ',';
            }
            output << "\"" << jsonEscape(parameterNames[index]) << "\"";
        }
        output << "]}";
    }
    output << "]}";
    return output.str();
}

bool ModuleDebugInspector::setDebugLevel(const std::string& module, const std::string& value, std::string& errorMessage)
{
    if (!isValidModuleName(module)) {
        errorMessage = "invalid module name";
        return false;
    }

    const std::string trimmedValue = trim(value);
    const size_t digitsStart = (!trimmedValue.empty() && trimmedValue[0] == '-') ? 1 : 0;
    const bool isInteger = trimmedValue.size() > digitsStart &&
                           std::all_of(trimmedValue.begin() + digitsStart, trimmedValue.end(), [](char character) { return std::isdigit(static_cast<unsigned char>(character)) != 0; });
    if (!isInteger) {
        errorMessage = "value must be an integer";
        return false;
    }

    const std::filesystem::path debugPath = std::filesystem::path("/sys/module") / module / "parameters" / "debug";
    std::error_code statusError;
    if (!std::filesystem::is_regular_file(debugPath, statusError) || statusError) {
        errorMessage = "module debug parameter not found";
        return false;
    }

    std::ofstream stream(debugPath);
    if (!stream) {
        errorMessage = "debug parameter is not writable";
        return false;
    }
    stream << trimmedValue;
    stream.flush();
    if (!stream) {
        errorMessage = "failed to write debug parameter";
        return false;
    }
    return true;
}
