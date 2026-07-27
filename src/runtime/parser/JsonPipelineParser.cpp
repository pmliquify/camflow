// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "JsonPipelineParser.hpp"

#include "pipeline/NodeTypeName.hpp"

#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>

static std::string trimJsonString(const std::string& value)
{
    size_t first = value.find_first_not_of(" \t\r\n\"");
    size_t last = value.find_last_not_of(" \t\r\n\"");
    if (first == std::string::npos || last == std::string::npos) {
        return std::string();
    }
    return value.substr(first, last - first + 1);
}

ParameterValue JsonPipelineParser::parseValue(const std::string& value) const
{
    std::string trimmed = trimJsonString(value);
    if (trimmed == "true") {
        return true;
    }
    if (trimmed == "false") {
        return false;
    }
    if (value.find('"') != std::string::npos) {
        return trimmed;
    }
    if (trimmed.find('.') != std::string::npos) {
        return std::stod(trimmed);
    }
    return static_cast<int64_t>(std::stoll(trimmed));
}

bool JsonPipelineParser::parseFile(const std::string& fileName, GraphConfig& config, std::string& errorMessage) const
{
    std::ifstream file(fileName);
    if (!file) {
        errorMessage = "Could not open graph json file: " + fileName;
        return false;
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    return parseText(stream.str(), config, errorMessage);
}

bool JsonPipelineParser::parseText(const std::string& text, GraphConfig& config, std::string& errorMessage) const
{
    GraphConfig result;
    std::regex nodeRegex(R"JSON(\{\s*"id"\s*:\s*"([^"]+)"\s*,\s*"type"\s*:\s*"([^"]+)"\s*(?:,\s*"parameters"\s*:\s*\{([^}]*)\})?\s*\})JSON");
    std::regex parameterRegex(R"JSON("([^"]+)"\s*:\s*("[^"]*"|true|false|-?[0-9]+(?:\.[0-9]+)?))JSON");
    std::regex edgeRegex(R"JSON("([A-Za-z0-9_]+)(?:\.([A-Za-z0-9_]+))?\s*->\s*([A-Za-z0-9_]+)(?:\.([A-Za-z0-9_]+))?")JSON");

    auto nodesBegin = std::sregex_iterator(text.begin(), text.end(), nodeRegex);
    auto nodesEnd = std::sregex_iterator();
    for (auto it = nodesBegin; it != nodesEnd; ++it) {
        NodeConfig node;
        node.id = (*it)[1].str();
        node.type = normalizeNodeTypeName((*it)[2].str());
        std::string parameters = (*it)[3].str();

        auto paramsBegin = std::sregex_iterator(parameters.begin(), parameters.end(), parameterRegex);
        for (auto pit = paramsBegin; pit != nodesEnd; ++pit) {
            node.parameters.set((*pit)[1].str(), parseValue((*pit)[2].str()));
        }
        result.addNode(node);
    }

    auto edgesBegin = std::sregex_iterator(text.begin(), text.end(), edgeRegex);
    for (auto it = edgesBegin; it != nodesEnd; ++it) {
        result.addEdge({(*it)[1].str(), (*it)[2].str().empty() ? "output" : (*it)[2].str(), (*it)[3].str(), (*it)[4].str().empty() ? "input" : (*it)[4].str()});
    }

    if (result.empty()) {
        errorMessage = "No nodes found in graph json";
        return false;
    }
    config = result;
    return true;
}
