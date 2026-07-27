// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "CLIPipelineParser.hpp"

#include "pipeline/NodeTypeName.hpp"

#include <algorithm>
#include <cctype>

std::string CLIPipelineParser::trim(const std::string& text) const
{
    size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::vector<std::string> CLIPipelineParser::splitTopLevel(const std::string& text, char delimiter) const
{
    std::vector<std::string> result;
    int parenDepth = 0;
    int braceDepth = 0;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    size_t start = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        const bool escaped = i > 0 && text[i - 1] == '\\';

        if (!escaped && c == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
            continue;
        }
        if (!escaped && c == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            continue;
        }
        if (inSingleQuote || inDoubleQuote) {
            continue;
        }

        if (c == '(') {
            ++parenDepth;
        } else if (c == ')') {
            --parenDepth;
        } else if (c == '{') {
            ++braceDepth;
        } else if (c == '}') {
            --braceDepth;
        } else if (parenDepth == 0 && braceDepth == 0 && c == delimiter) {
            result.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    result.push_back(text.substr(start));
    return result;
}

bool CLIPipelineParser::isWrappedByParentheses(const std::string& text) const
{
    if (text.size() < 2 || text.front() != '(' || text.back() != ')') {
        return false;
    }

    int depth = 0;
    int braceDepth = 0;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        const bool escaped = i > 0 && text[i - 1] == '\\';

        if (!escaped && c == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
            continue;
        }
        if (!escaped && c == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            continue;
        }

        if (inSingleQuote || inDoubleQuote) {
            continue;
        }

        if (c == '{') {
            ++braceDepth;
        } else if (c == '}') {
            --braceDepth;
        } else if (c == '(') {
            ++depth;
        } else if (c == ')') {
            --depth;
            if (depth == 0 && i != text.size() - 1) {
                return false;
            }
        }
    }

    return depth == 0 && braceDepth == 0 && !inSingleQuote && !inDoubleQuote;
}

bool CLIPipelineParser::tokenizeChain(const std::string& text, std::vector<std::string>& segments, std::vector<LinkToken>& links, std::string& errorMessage) const
{
    int parenDepth = 0;
    int braceDepth = 0;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    size_t start = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        const bool escaped = i > 0 && text[i - 1] == '\\';

        if (!escaped && c == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
            continue;
        }
        if (!escaped && c == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            continue;
        }
        if (inSingleQuote || inDoubleQuote) {
            continue;
        }

        if (c == '(') {
            ++parenDepth;
            continue;
        }
        if (c == ')') {
            --parenDepth;
            if (parenDepth < 0) {
                errorMessage = "Unbalanced ')' in pipeline expression";
                return false;
            }
            continue;
        }
        if (c == '{') {
            ++braceDepth;
            continue;
        }
        if (c == '}') {
            --braceDepth;
            if (braceDepth < 0) {
                errorMessage = "Unbalanced '}' in pipeline expression";
                return false;
            }
            continue;
        }
        if (parenDepth != 0 || braceDepth != 0 || c != '-') {
            continue;
        }

        if (i + 1 < text.size() && text[i + 1] == '>') {
            segments.push_back(text.substr(start, i - start));
            links.push_back({false, std::string()});
            i += 1;
            start = i + 1;
            continue;
        }

        size_t probeBegin = i + 1;
        size_t probeEnd = probeBegin;
        while (probeEnd < text.size() && text[probeEnd] != '>') {
            ++probeEnd;
        }
        if (probeEnd < text.size() && text[probeEnd] == '>') {
            const std::string probeText = trim(text.substr(probeBegin, probeEnd - probeBegin));
            if (probeText.empty()) {
                errorMessage = "Probe operator requires a non-empty probe id";
                return false;
            }
            segments.push_back(text.substr(start, i - start));
            links.push_back({true, probeText});
            i = probeEnd;
            start = i + 1;
            continue;
        }
    }

    if (parenDepth != 0 || braceDepth != 0 || inSingleQuote || inDoubleQuote) {
        errorMessage = "Unbalanced delimiters in pipeline expression";
        return false;
    }

    segments.push_back(text.substr(start));
    return true;
}

ParameterValue CLIPipelineParser::parseParameterValue(const std::string& text) const
{
    std::string value = trim(text);
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) {
        return value.substr(1, value.size() - 2);
    }
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }

    bool hasDot = false;
    size_t start = (value.size() > 1 && (value[0] == '+' || value[0] == '-')) ? 1U : 0U;
    bool numeric = !value.empty();
    for (size_t i = start; i < value.size(); ++i) {
        const char c = value[i];
        if (c == '.') {
            if (hasDot) {
                numeric = false;
                break;
            }
            hasDot = true;
            continue;
        }
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            numeric = false;
            break;
        }
    }

    if (numeric && !value.empty()) {
        try {
            if (hasDot) {
                return std::stod(value);
            }
            return static_cast<int64_t>(std::stoll(value));
        } catch (...) {
            return value;
        }
    }

    return value;
}

void CLIPipelineParser::parseParameters(const std::string& text, ParameterSet& parameters) const
{
    std::string lastName;
    for (const auto& rawItem : splitTopLevel(text, ',')) {
        std::string item = trim(rawItem);
        if (item.empty()) {
            continue;
        }

        size_t equalPos = item.find('=');
        if (equalPos == std::string::npos) {
            if (!lastName.empty()) {
                const ParameterValue* current = parameters.value(lastName);
                const std::string merged = (current == nullptr ? std::string() : parameterValueToString(*current) + ",") + item;
                parameters.set(lastName, merged);
            } else {
                parameters.set(item, true);
            }
            continue;
        }

        std::string name = trim(item.substr(0, equalPos));
        std::string value = trim(item.substr(equalPos + 1));
        if (!name.empty()) {
            parameters.set(name, parseParameterValue(value));
            lastName = name;
        }
    }
}

NodeConfig CLIPipelineParser::parseNode(const std::string& text, ParseContext& context, std::string& errorMessage) const
{
    NodeConfig node;
    std::string value = trim(text);

    if (value.empty()) {
        errorMessage = "Empty node declaration in pipeline expression";
        return {};
    }

    size_t paramsBegin = std::string::npos;
    int depth = 0;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '(') {
            if (depth == 0) {
                paramsBegin = i;
                break;
            }
            ++depth;
        } else if (value[i] == ')') {
            --depth;
        }
    }

    std::string header = value;
    std::string params;
    if (paramsBegin != std::string::npos) {
        size_t paramsEnd = value.rfind(')');
        if (paramsEnd == std::string::npos || paramsEnd < paramsBegin) {
            errorMessage = "Unbalanced node parameter parentheses: " + value;
            return {};
        }
        if (trim(value.substr(paramsEnd + 1)).size() != 0) {
            errorMessage = "Unexpected token after node parameter list: " + value;
            return {};
        }
        header = trim(value.substr(0, paramsBegin));
        params = value.substr(paramsBegin + 1, paramsEnd - paramsBegin - 1);
    }

    if (header.find('{') != std::string::npos || header.find('}') != std::string::npos) {
        errorMessage = "Node scope selectors '{...}' are no longer supported. Bind inputs via <input>=<nodeId>.<output>.";
        return {};
    }

    size_t colonPos = header.find(':');
    if (colonPos != std::string::npos) {
        node.id = trim(header.substr(0, colonPos));
        node.type = normalizeNodeTypeName(trim(header.substr(colonPos + 1)));
    } else {
        node.type = normalizeNodeTypeName(trim(header));
        node.id = makeAutomaticNodeId(node.type, context.nodeIndex++);
    }

    if (node.id.empty() || node.type.empty()) {
        errorMessage = "Invalid node declaration: " + value;
        return {};
    }

    parseParameters(params, node.parameters);
    return node;
}

bool CLIPipelineParser::isSinkType(const std::string& type) const
{
    const std::string normalized = normalizeNodeTypeName(type);
    return normalized.size() >= 4 && normalized.substr(normalized.size() - 4) == "sink";
}

bool CLIPipelineParser::addNodeAndConnect(const NodeConfig& node, const std::vector<std::string>& inputFrontier, std::vector<std::string>& outputFrontier, GraphConfig& config, ParseContext& context,
                                          std::string& errorMessage) const
{
    if (!context.usedIds.insert(node.id).second) {
        errorMessage = "Duplicate node id in pipeline expression: " + node.id;
        return false;
    }

    config.addNode(node);
    for (const auto& from : inputFrontier) {
        config.addEdge({from, "output", node.id, "input"});
    }

    if (isSinkType(node.type)) {
        outputFrontier.clear();
    } else {
        outputFrontier = {node.id};
    }
    return true;
}

bool CLIPipelineParser::addProbeNode(const std::string& probeId, const std::vector<std::string>& inputFrontier, std::vector<std::string>& outputFrontier, GraphConfig& config, ParseContext& context,
                                     std::string& errorMessage) const
{
    if (inputFrontier.empty()) {
        errorMessage = "Probe operator requires a node before it";
        return false;
    }

    NodeConfig probeNode;
    probeNode.type = "probe";
    probeNode.id = "probe" + std::to_string(context.probeIndex++);
    probeNode.parameters.set("id", probeId);

    return addNodeAndConnect(probeNode, inputFrontier, outputFrontier, config, context, errorMessage);
}

bool CLIPipelineParser::parseSegment(const std::string& text, const std::vector<std::string>& inputFrontier, std::vector<std::string>& outputFrontier, GraphConfig& config, ParseContext& context,
                                     std::string& errorMessage) const
{
    const std::string segment = trim(text);
    if (segment.empty()) {
        errorMessage = "Empty segment in pipeline expression";
        return false;
    }

    if (isWrappedByParentheses(segment)) {
        const std::string inner = segment.substr(1, segment.size() - 2);
        std::vector<std::string> mergedFrontier;

        for (const auto& rawBranch : splitTopLevel(inner, ',')) {
            const std::string branch = trim(rawBranch);
            if (branch.empty()) {
                errorMessage = "Empty branch inside grouping";
                return false;
            }

            std::vector<std::string> branchOutput;
            if (!parseChain(branch, inputFrontier, branchOutput, config, context, errorMessage)) {
                return false;
            }
            for (const auto& id : branchOutput) {
                if (std::find(mergedFrontier.begin(), mergedFrontier.end(), id) == mergedFrontier.end()) {
                    mergedFrontier.push_back(id);
                }
            }
        }

        outputFrontier = mergedFrontier;
        return true;
    }

    NodeConfig node = parseNode(segment, context, errorMessage);
    if (!errorMessage.empty()) {
        return false;
    }
    return addNodeAndConnect(node, inputFrontier, outputFrontier, config, context, errorMessage);
}

bool CLIPipelineParser::parseChain(const std::string& text, const std::vector<std::string>& inputFrontier, std::vector<std::string>& outputFrontier, GraphConfig& config, ParseContext& context,
                                   std::string& errorMessage) const
{
    std::vector<std::string> segments;
    std::vector<LinkToken> links;
    if (!tokenizeChain(text, segments, links, errorMessage)) {
        return false;
    }

    std::vector<std::string> currentFrontier = inputFrontier;
    for (size_t i = 0; i < segments.size(); ++i) {
        const std::string segment = trim(segments[i]);
        if (segment.empty()) {
            const bool isTrailingSegment = (i + 1 == segments.size());
            const bool previousWasProbeLink = isTrailingSegment && !links.empty() && links.back().hasProbe;
            if (previousWasProbeLink) {
                continue;
            }
            errorMessage = "Empty segment in pipeline expression";
            return false;
        }

        std::vector<std::string> nextFrontier;
        if (!parseSegment(segment, currentFrontier, nextFrontier, config, context, errorMessage)) {
            return false;
        }
        currentFrontier = nextFrontier;

        if (i < links.size() && links[i].hasProbe) {
            std::vector<std::string> probeFrontier;
            if (!addProbeNode(links[i].probeId, currentFrontier, probeFrontier, config, context, errorMessage)) {
                return false;
            }
            currentFrontier = probeFrontier;
        }
    }

    outputFrontier = currentFrontier;
    return true;
}

bool CLIPipelineParser::parseTopLevel(const std::string& text, GraphConfig& config, ParseContext& context, std::string& errorMessage) const
{
    for (const auto& rawPipeline : splitTopLevel(text, ',')) {
        const std::string pipeline = trim(rawPipeline);
        if (pipeline.empty()) {
            continue;
        }

        std::vector<std::string> unusedFrontier;
        if (!parseChain(pipeline, {}, unusedFrontier, config, context, errorMessage)) {
            return false;
        }
    }

    return true;
}

bool CLIPipelineParser::parse(const std::string& text, GraphConfig& config, std::string& errorMessage) const
{
    errorMessage.clear();
    config = GraphConfig();

    ParseContext context;
    if (!parseTopLevel(text, config, context, errorMessage)) {
        return false;
    }

    if (config.empty()) {
        errorMessage = "Pipeline expression is empty";
        return false;
    }

    return true;
}
