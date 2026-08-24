// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "RestApiInterface.hpp"

#include "core/Logger.hpp"
#include "media/MediaGraphInspector.hpp"
#include "parameters/Parameter.hpp"
#include "parser/JsonPipelineParser.hpp"
#include "system/DeviceTreeInspector.hpp"

#include "version.h"

#include <opencv2/core/version.hpp>

#include <algorithm>
#include <cstdlib>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <variant>

namespace
{

std::string jsonEscape(const std::string& value)
{
    std::string result;
    for (char c : value) {
        if (c == '"') {
            result += "\\\"";
        } else if (c == '\\') {
            result += "\\\\";
        } else if (c == '\n') {
            result += "\\n";
        } else if (c == '\r') {
            result += "\\r";
        } else {
            result += c;
        }
    }
    return result;
}

std::string parameterTypeToJson(ParameterType type)
{
    switch (type) {
    case ParameterType::Int:
        return "int";
    case ParameterType::Double:
        return "double";
    case ParameterType::Bool:
        return "bool";
    case ParameterType::String:
        return "string";
    case ParameterType::Option:
        return "option";
    case ParameterType::Button:
        return "button";
    }
    return "unknown";
}

std::string valueToJsonRest(const ParameterValue& value)
{
    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? "true" : "false";
    }
    if (std::holds_alternative<int64_t>(value)) {
        return std::to_string(std::get<int64_t>(value));
    }
    if (std::holds_alternative<double>(value)) {
        std::ostringstream stream;
        stream << std::get<double>(value);
        return stream.str();
    }
    return "\"" + jsonEscape(std::get<std::string>(value)) + "\"";
}

std::string schemaToJson(const NodeSchema& schema, const ParameterSet& configured)
{
    std::ostringstream json;
    json << "{\"parameters\":[";
    bool first = true;
    for (const auto& parameter : schema.parameters) {
        if (!first) {
            json << ",";
        }
        first = false;

        const ParameterValue* configuredValue = configured.value(parameter.name);
        const ParameterValue& effectiveValue = configuredValue == nullptr ? parameter.defaultValue : *configuredValue;

        json << "{\"name\":\"" << jsonEscape(parameter.name) << "\",";
        json << "\"type\":\"" << parameterTypeToJson(parameter.type) << "\",";
        json << "\"description\":\"" << jsonEscape(parameter.description) << "\",";
        json << "\"default\":" << valueToJsonRest(parameter.defaultValue) << ",";
        json << "\"min\":" << valueToJsonRest(parameter.minimumValue) << ",";
        json << "\"max\":" << valueToJsonRest(parameter.maximumValue) << ",";
        json << "\"runtimeWritable\":" << (parameter.runtimeWritable ? "true" : "false") << ",";
        json << "\"readOnly\":" << (parameter.readOnly ? "true" : "false") << ",";
        json << "\"hasSideEffects\":" << (parameter.hasSideEffects ? "true" : "false") << ",";
        json << "\"multiSelect\":" << (parameter.multiSelect ? "true" : "false") << ",";
        json << "\"configured\":" << (configured.contains(parameter.name) ? "true" : "false") << ",";
        json << "\"value\":" << valueToJsonRest(effectiveValue);

        if (!parameter.options.empty()) {
            json << ",\"options\":[";
            for (size_t i = 0; i < parameter.options.size(); ++i) {
                if (i) {
                    json << ",";
                }
                json << "\"" << jsonEscape(parameter.options[i]) << "\"";
            }
            json << "]";
        }

        if (!parameter.optionLabels.empty()) {
            json << ",\"optionLabels\":[";
            for (size_t i = 0; i < parameter.optionLabels.size(); ++i) {
                if (i) {
                    json << ",";
                }
                json << "\"" << jsonEscape(parameter.optionLabels[i]) << "\"";
            }
            json << "]";
        }

        if (!parameter.origin.empty()) {
            json << ",\"origin\":\"" << jsonEscape(parameter.origin) << "\"";
        }

        if (!parameter.source.empty()) {
            json << ",\"source\":\"" << jsonEscape(parameter.source) << "\"";
        }

        if (!parameter.group.empty()) {
            json << ",\"group\":\"" << jsonEscape(parameter.group) << "\"";
        }

        if (!parameter.groupDescription.empty()) {
            json << ",\"groupDescription\":\"" << jsonEscape(parameter.groupDescription) << "\"";
        }

        json << "}";
    }
    json << "],\"inputs\":[";
    for (size_t i = 0; i < schema.inputs.size(); ++i) {
        if (i) {
            json << ",";
        }
        const auto& input = schema.inputs[i];
        json << "{\"name\":\"" << jsonEscape(input.name) << "\",";
        json << "\"type\":\"" << jsonEscape(input.dataType) << "\",";
        json << "\"description\":\"" << jsonEscape(input.description) << "\",";
        json << "\"allowMultipleBindings\":" << (input.allowMultipleBindings ? "true" : "false") << "}";
    }
    json << "],\"outputs\":[";
    for (size_t i = 0; i < schema.outputs.size(); ++i) {
        if (i) {
            json << ",";
        }
        const auto& output = schema.outputs[i];
        json << "{\"name\":\"" << jsonEscape(output.name) << "\",";
        json << "\"type\":\"" << jsonEscape(output.dataType) << "\",";
        json << "\"description\":\"" << jsonEscape(output.description) << "\"}";
    }
    json << "]}";
    return json.str();
}

std::string trim(const std::string& text)
{
    size_t start = 0;
    while (start < text.size() && (text[start] == ' ' || text[start] == '\t' || text[start] == '\r' || text[start] == '\n')) {
        ++start;
    }
    size_t end = text.size();
    while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r' || text[end - 1] == '\n')) {
        --end;
    }
    return text.substr(start, end - start);
}

std::string requestPathWithoutQuery(const std::string& path)
{
    return path.substr(0, path.find('?'));
}

std::string decodeUrl(const std::string& value)
{
    std::string decoded;
    decoded.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            const std::string hex = value.substr(i + 1, 2);
            char* end = nullptr;
            const long code = std::strtol(hex.c_str(), &end, 16);
            if (end != nullptr && *end == '\0') {
                decoded.push_back(static_cast<char>(code));
                i += 2;
                continue;
            }
        }
        decoded.push_back(value[i] == '+' ? ' ' : value[i]);
    }
    return decoded;
}

bool parseDesiredRuntimeStopped(const std::string& body, bool& stopped)
{
    std::string clean = trim(body);
    if (clean.empty()) {
        return false;
    }

    std::smatch match;
    const std::regex jsonRegex("\\\"desiredState\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
    if (std::regex_search(clean, match, jsonRegex) && match.size() > 1) {
        clean = match[1].str();
    } else if (!clean.empty() && clean.front() == '"' && clean.back() == '"' && clean.size() > 1) {
        clean = clean.substr(1, clean.size() - 2);
    }

    if (clean == "running") {
        stopped = false;
        return true;
    }
    if (clean == "stopped") {
        stopped = true;
        return true;
    }
    return false;
}

bool hasAutoNodeIdMarker(const std::string& id)
{
    return id == "__auto__";
}

void assignAutomaticNodeIds(GraphConfig& config)
{
    std::unordered_set<std::string> usedIds;
    std::unordered_map<std::string, std::string> rewrittenIds;

    for (const auto& node : config.nodes()) {
        if (!node.id.empty() && !hasAutoNodeIdMarker(node.id)) {
            usedIds.insert(node.id);
        }
    }

    std::unordered_map<std::string, int> nextByType;
    for (auto& node : config.nodes()) {
        if (!node.id.empty() && !hasAutoNodeIdMarker(node.id)) {
            continue;
        }

        const std::string baseType = node.type.empty() ? "node" : node.type;
        int index = nextByType[baseType];
        std::string generated;
        do {
            generated = baseType + std::to_string(index++);
        } while (usedIds.find(generated) != usedIds.end());

        nextByType[baseType] = index;
        usedIds.insert(generated);
        rewrittenIds[node.id] = generated;
        node.id = generated;
    }

    if (rewrittenIds.empty()) {
        return;
    }

    for (auto& edge : config.edges()) {
        auto fromIt = rewrittenIds.find(edge.fromNode);
        if (fromIt != rewrittenIds.end()) {
            edge.fromNode = fromIt->second;
        }
        auto toIt = rewrittenIds.find(edge.toNode);
        if (toIt != rewrittenIds.end()) {
            edge.toNode = toIt->second;
        }
    }
}

bool parseSingleNodePayload(const std::string& body, NodeConfig& node, std::string& errorMessage)
{
    JsonPipelineParser parser;
    GraphConfig config;
    const std::string wrapped = std::string{"{\"nodes\":["} + body + "],\"edges\":[]}";
    if (!parser.parseText(wrapped, config, errorMessage)) {
        return false;
    }
    if (config.nodes().size() != 1) {
        errorMessage = "expected exactly one node";
        return false;
    }
    node = config.nodes().front();

    auto extractOptionalStringField = [&](const char* fieldName) {
        const std::regex fieldRegex(std::string{"\""} + fieldName + "\"\\s*:\\s*\"([^\"]*)\"");
        std::smatch match;
        if (std::regex_search(body, match, fieldRegex) && match.size() > 1) {
            return match[1].str();
        }
        return std::string();
    };

    std::string runtimeTarget = extractOptionalStringField("runtimeTargetIp");
    if (runtimeTarget.empty()) {
        runtimeTarget = extractOptionalStringField("runtimeId");
    }
    if (runtimeTarget.empty()) {
        runtimeTarget = extractOptionalStringField("runtime");
    }
    if (!runtimeTarget.empty()) {
        node.parameters.set("runtimeTargetIp", runtimeTarget);
    }

    return true;
}

bool parseSingleEdgePayload(const std::string& body, EdgeConfig& edge, std::string& errorMessage)
{
    std::string text = trim(body);

    if (text.empty() || text.front() != '{' || text.back() != '}') {
        errorMessage = "expected JSON object with edge fields";
        return false;
    }

    auto extractField = [&](const char* name) {
        const std::regex fieldRegex(std::string{"\""} + name + "\"\\s*:\\s*\"([^\"]*)\"");
        std::smatch match;
        if (std::regex_search(text, match, fieldRegex) && match.size() > 1) {
            return trim(match[1].str());
        }
        return std::string();
    };

    edge.fromNode = extractField("fromNode");
    edge.fromPort = extractField("fromPort");
    edge.toNode = extractField("toNode");
    edge.toPort = extractField("toPort");

    if (edge.fromNode.empty() || edge.toNode.empty()) {
        const std::string from = extractField("from");
        const std::string to = extractField("to");
        auto parseEndpoint = [](const std::string& endpoint, std::string& node, std::string& port) {
            const auto dot = endpoint.rfind('.');
            if (dot == std::string::npos || dot == 0 || dot + 1 >= endpoint.size()) {
                node = trim(endpoint);
                port.clear();
                return;
            }
            node = trim(endpoint.substr(0, dot));
            port = trim(endpoint.substr(dot + 1));
        };
        parseEndpoint(from, edge.fromNode, edge.fromPort);
        parseEndpoint(to, edge.toNode, edge.toPort);
    }

    edge.fromNode = trim(edge.fromNode);
    edge.toNode = trim(edge.toNode);
    edge.fromPort = trim(edge.fromPort);
    edge.toPort = trim(edge.toPort);

    if (edge.fromNode.empty() || edge.toNode.empty()) {
        errorMessage = "edge JSON must contain fromNode/toNode or from/to";
        return false;
    }

    if (edge.fromPort.empty() || edge.fromPort == "output") {
        edge.fromPort = "image";
    }
    if (edge.toPort.empty() || edge.toPort == "input") {
        edge.toPort = "image";
    }
    return true;
}

} // namespace

RestApiInterface::RestApiInterface(const NodeFactory& factory, RuntimeController& controller, bool uiModeEnabled) :
    m_factory(factory),
    m_controller(controller),
    m_uiModeEnabled(uiModeEnabled)
{
}

bool RestApiInterface::tryHandle(const std::string& method, const std::string& path, const std::string& body, std::string& responseBody, std::string& contentType, int& statusCode) const
{
    const std::string requestPath = requestPathWithoutQuery(path);

    if (method == "GET" && requestPath == "/api/runtime") {
        statusCode = 200;
        contentType = "application/json";
        responseBody = runtimeStateJson();
        return true;
    }

    if (method == "GET" && requestPath == "/api/runtime/version") {
        statusCode = 200;
        contentType = "application/json";
        responseBody = runtimeVersionJson();
        return true;
    }

    if (method == "GET" && requestPath == "/api/media") {
        statusCode = 200;
        contentType = "application/json";
        responseBody = MediaGraphInspector::devicesJson();
        return true;
    }

    if (method == "GET" && requestPath.rfind("/api/media/", 0) == 0) {
        const std::string device = "/dev/" + decodeUrl(requestPath.substr(11));
        std::string errorMessage;
        if (!MediaGraphInspector::graphJson(device, responseBody, errorMessage)) {
            statusCode = errorMessage == "media device not found" ? 404 : 500;
            contentType = "application/json";
            responseBody = "{\"error\":\"" + jsonEscape(errorMessage) + "\"}";
            return true;
        }
        statusCode = 200;
        contentType = "application/json";
        return true;
    }

    if (method == "GET" && requestPath == "/api/devicetree") {
        std::string errorMessage;
        if (!DeviceTreeInspector::treeJson(responseBody, errorMessage)) {
            statusCode = errorMessage == "device tree not available" ? 404 : 500;
            contentType = "application/json";
            responseBody = "{\"error\":\"" + jsonEscape(errorMessage) + "\"}";
            return true;
        }
        statusCode = 200;
        contentType = "application/json";
        return true;
    }

    if (method == "PUT" && requestPath == "/api/runtime") {
        bool stopped = false;
        if (!parseDesiredRuntimeStopped(body, stopped)) {
            statusCode = 400;
            contentType = "application/json";
            responseBody = "{\"ok\":false,\"error\":\"expected desiredState running or stopped\"}";
            return true;
        }
        if (!m_controller.setStopped(stopped)) {
            statusCode = 404;
            contentType = "application/json";
            responseBody = "{\"ok\":false}";
            return true;
        }
        statusCode = 200;
        contentType = "application/json";
        responseBody = runtimeStateJson();
        return true;
    }

    if (method == "GET" && requestPath == "/api/pipeline") {
        statusCode = 200;
        contentType = "application/json";
        responseBody = m_controller.graphJson();
        return true;
    }

    if ((method == "PUT" || method == "POST") && requestPath == "/api/pipeline") {
        JsonPipelineParser parser;
        GraphConfig config;
        std::string errorMessage;
        if (!parser.parseText(body, config, errorMessage)) {
            statusCode = 400;
            contentType = "application/json";
            responseBody = "{\"ok\":false,\"error\":\"" + jsonEscape(errorMessage) + "\"}";
            return true;
        }

        assignAutomaticNodeIds(config);

        if (!m_controller.replaceGraph(config)) {
            statusCode = 500;
            contentType = "application/json";
            responseBody = "{\"ok\":false}";
            return true;
        }

        statusCode = 200;
        contentType = "application/json";
        responseBody = "{\"ok\":true}";
        return true;
    }

    if ((method == "PUT" || method == "POST") && requestPath == "/api/nodes") {
        NodeConfig node;
        std::string errorMessage;
        if (!parseSingleNodePayload(body, node, errorMessage)) {
            statusCode = 400;
            contentType = "application/json";
            responseBody = "{\"ok\":false,\"error\":\"" + jsonEscape(errorMessage) + "\"}";
            return true;
        }

        if (!m_controller.addNode(node)) {
            statusCode = 409;
            contentType = "application/json";
            responseBody = "{\"ok\":false}";
            return true;
        }

        statusCode = 200;
        contentType = "application/json";
        responseBody = "{\"ok\":true,\"id\":\"" + jsonEscape(node.id) + "\"}";
        return true;
    }

    if ((method == "PUT" || method == "POST") && requestPath == "/api/edges") {
        EdgeConfig edge;
        std::string errorMessage;
        if (!parseSingleEdgePayload(body, edge, errorMessage)) {
            statusCode = 400;
            contentType = "application/json";
            responseBody = "{\"ok\":false,\"error\":\"" + jsonEscape(errorMessage) + "\"}";
            return true;
        }

        if (!m_controller.addEdge(edge)) {
            statusCode = 409;
            contentType = "application/json";
            responseBody = "{\"ok\":false}";
            return true;
        }

        statusCode = 200;
        contentType = "application/json";
        responseBody = "{\"ok\":true}";
        return true;
    }

    if (method == "DELETE" && requestPath == "/api/edges") {
        EdgeConfig edge;
        std::string errorMessage;
        if (!parseSingleEdgePayload(body, edge, errorMessage)) {
            statusCode = 400;
            contentType = "application/json";
            responseBody = "{\"ok\":false,\"error\":\"" + jsonEscape(errorMessage) + "\"}";
            return true;
        }

        if (!m_controller.removeEdge(edge)) {
            statusCode = 409;
            contentType = "application/json";
            responseBody = "{\"ok\":false}";
            return true;
        }

        statusCode = 200;
        contentType = "application/json";
        responseBody = "{\"ok\":true}";
        return true;
    }

    if (method == "PUT" && requestPath.rfind("/api/nodes/", 0) == 0 && requestPath.size() > 14 && requestPath.compare(requestPath.size() - 3, 3, "/id") == 0) {
        const std::string nodeId = decodeUrl(requestPath.substr(11, requestPath.size() - 14));
        const std::string newNodeId = trim(body);
        if (!m_controller.renameNode(nodeId, newNodeId)) {
            statusCode = 409;
            contentType = "application/json";
            responseBody = "{\"ok\":false}";
            return true;
        }

        statusCode = 200;
        contentType = "application/json";
        responseBody = "{\"ok\":true,\"id\":\"" + jsonEscape(newNodeId) + "\"}";
        return true;
    }

    if (method == "DELETE" && requestPath.rfind("/api/nodes/", 0) == 0 && requestPath.find("/parameters") == std::string::npos) {
        const std::string nodeId = decodeUrl(requestPath.substr(11));
        if (!m_controller.removeNode(nodeId)) {
            statusCode = 409;
            contentType = "application/json";
            responseBody = "{\"ok\":false}";
            return true;
        }

        statusCode = 200;
        contentType = "application/json";
        responseBody = "{\"ok\":true}";
        return true;
    }

    if (method == "GET" && requestPath == "/api/nodes") {
        std::ostringstream json;
        auto writeGroup = [&](const char* name, NodeKind kind) {
            json << "\"" << name << "\":[";
            auto types = m_factory.registeredTypes(kind);
            for (size_t i = 0; i < types.size(); ++i) {
                if (i) {
                    json << ",";
                }
                json << "\"" << jsonEscape(types[i]) << "\"";
            }
            json << "]";
        };
        json << "{";
        writeGroup("sources", NodeKind::Source);
        json << ",";
        writeGroup("processors", NodeKind::Processor);
        json << ",";
        writeGroup("sinks", NodeKind::Sink);
        json << ",\"schemas\":{";
        bool firstSchema = true;
        for (const auto kind : {NodeKind::Source, NodeKind::Processor, NodeKind::Sink}) {
            for (const auto& type : m_factory.registeredTypes(kind)) {
                if (!firstSchema) {
                    json << ",";
                }
                firstSchema = false;
                json << "\"" << jsonEscape(type) << "\":" << schemaToJson(m_factory.schema(type), ParameterSet{});
            }
        }
        json << "}";
        json << "}";

        statusCode = 200;
        contentType = "application/json";
        responseBody = json.str();
        return true;
    }

    if (method == "GET" && requestPath.rfind("/api/nodes/", 0) == 0 && requestPath.find("/parameters") != std::string::npos) {
        const std::string nodeId = decodeUrl(requestPath.substr(11, requestPath.find("/parameters") - 11));
        NodeSchema schema;
        ParameterSet parameters;
        if (m_controller.nodeState(nodeId, schema, parameters)) {
            statusCode = 200;
            contentType = "application/json";
            responseBody = schemaToJson(schema, parameters);
            return true;
        }

        NodeConfig config;
        if (!m_controller.nodeConfig(nodeId, config)) {
            statusCode = 404;
            contentType = "application/json";
            responseBody = "{\"error\":\"node not found\"}";
            return true;
        }

        statusCode = 200;
        contentType = "application/json";
        responseBody = schemaToJson(m_factory.schema(config.type), config.parameters);
        return true;
    }

    if ((method == "POST" || method == "PUT") && requestPath.rfind("/api/nodes/", 0) == 0 && requestPath.find("/parameters/") != std::string::npos) {
        const size_t parameterPos = requestPath.find("/parameters/");
        const std::string nodeId = decodeUrl(requestPath.substr(11, parameterPos - 11));
        const std::string parameterName = decodeUrl(requestPath.substr(parameterPos + 12));
        const std::string target = nodeId + "." + parameterName;

        LOG_INFO("REST parameter request: " + method + " " + requestPath + " value=" + body);

        NodeSchema schema;
        ParameterSet parameters;
        if (!m_controller.nodeState(nodeId, schema, parameters)) {
            statusCode = 404;
            contentType = "application/json";
            responseBody = "{\"ok\":false,\"error\":\"node not found\"}";
            LOG_WARNING("REST parameter response: 404 " + target + " (node not found)");
            return true;
        }

        const auto parameterIt = std::find_if(schema.parameters.begin(), schema.parameters.end(), [&parameterName](const ParameterInfo& parameter) { return parameter.name == parameterName; });
        if (parameterIt == schema.parameters.end()) {
            statusCode = 404;
            contentType = "application/json";
            responseBody = "{\"ok\":false,\"error\":\"parameter not found\"}";
            LOG_WARNING("REST parameter response: 404 " + target + " (parameter not found)");
            return true;
        }

        if (parameterIt->readOnly) {
            statusCode = 409;
            contentType = "application/json";
            responseBody = "{\"ok\":false,\"error\":\"parameter is read-only\"}";
            LOG_WARNING("REST parameter response: 409 " + target + " (read-only)");
            return true;
        }

        if (!parameterIt->runtimeWritable && !m_controller.isStopped()) {
            statusCode = 409;
            contentType = "application/json";
            responseBody = "{\"ok\":false,\"error\":\"parameter is not writable while runtime is running\"}";
            LOG_WARNING("REST parameter response: 409 " + target + " (not writable while runtime is running)");
            return true;
        }

        std::string errorMessage;
        const bool ok = m_controller.setParameterFromString(nodeId, parameterName, body, &errorMessage);

        statusCode = ok ? 200 : 422;
        contentType = "application/json";
        responseBody = ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"" + jsonEscape(errorMessage.empty() ? "parameter value was rejected" : errorMessage) + "\"}";
        if (ok) {
            LOG_INFO("REST parameter response: 200 " + target + " = " + body);
        } else {
            LOG_WARNING("REST parameter response: 422 " + target + " (" + (errorMessage.empty() ? "value rejected" : errorMessage) + ")");
        }
        return true;
    }

    return false;
}

std::string RestApiInterface::runtimeStateJson() const
{
    std::ostringstream json;
    const bool stopped = m_controller.isStopped();
    json << "{\"state\":\"" << (stopped ? "stopped" : "running") << "\",\"ui\":" << (m_uiModeEnabled ? "true" : "false") << "}";
    return json.str();
}

std::string RestApiInterface::runtimeVersionJson() const
{
    std::ostringstream json;
    json << "{\"version\":\"v" << CAMFLOW_VERSION << "\""
         << ",\"git\":\"" << CAMFLOW_GIT_COMMIT << "\""
         << ",\"build\":\"" << CAMFLOW_BUILD_TIMESTAMP << "\""
         << ",\"opencv\":\"" << CV_VERSION << "\""
         << "}";
    return json.str();
}
