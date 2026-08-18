// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "RuntimeController.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

static std::string jsonEscapeRuntime(const std::string& value)
{
    std::string result;
    for (char c : value) {
        if (c == '"') {
            result += "\\\"";
        } else if (c == '\\') {
            result += "\\\\";
        } else {
            result += c;
        }
    }
    return result;
}

static std::string valueToJson(const ParameterValue& value)
{
    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? "true" : "false";
    }
    if (std::holds_alternative<int64_t>(value)) {
        return std::to_string(std::get<int64_t>(value));
    }
    if (std::holds_alternative<double>(value)) {
        return std::to_string(std::get<double>(value));
    }
    return "\"" + jsonEscapeRuntime(std::get<std::string>(value)) + "\"";
}

static std::vector<std::string> splitCommaValues(const std::string& text)
{
    std::vector<std::string> values;
    std::string token;
    std::istringstream stream(text);
    while (std::getline(stream, token, ',')) {
        const auto begin = token.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) {
            continue;
        }
        const auto end = token.find_last_not_of(" \t\r\n");
        values.push_back(token.substr(begin, end - begin + 1));
    }
    return values;
}

static bool containsInputName(const NodeSchema& schema, const std::string& inputName)
{
    return std::any_of(schema.inputs.begin(), schema.inputs.end(), [&](const NodeInputInfo& input) { return input.name == inputName; });
}

static std::string generateUniqueNodeId(const GraphConfig& graph, const std::string& typeName)
{
    const std::string base = typeName.empty() ? "node" : typeName;
    size_t index = 0;
    while (true) {
        const std::string candidate = base + std::to_string(index);
        const bool exists = std::any_of(graph.nodes().begin(), graph.nodes().end(), [&](const NodeConfig& node) { return node.id == candidate; });
        if (!exists) {
            return candidate;
        }
        ++index;
    }
}

static bool applyEdgeBindingLocked(GraphConfig& graph, IPipeline* pipeline, const EdgeConfig& edge)
{
    NodeConfig* targetNodeConfig = graph.findNode(edge.toNode);
    if (targetNodeConfig == nullptr) {
        return false;
    }

    const std::string sourcePort = (edge.fromPort.empty() || edge.fromPort == "output") ? "image" : edge.fromPort;
    const std::string targetInput = (edge.toPort.empty() || edge.toPort == "input") ? "image" : edge.toPort;
    const std::string bindingToken = edge.fromNode + "." + sourcePort;

    Node* liveTarget = pipeline ? pipeline->findNode(edge.toNode) : nullptr;
    if (liveTarget != nullptr) {
        const NodeSchema schema = liveTarget->schema();
        if (!containsInputName(schema, targetInput)) {
            return false;
        }
    }

    const std::string currentBinding = targetNodeConfig->parameters.stringValue(targetInput, std::string());
    std::vector<std::string> tokens = splitCommaValues(currentBinding);
    if (std::find(tokens.begin(), tokens.end(), bindingToken) == tokens.end()) {
        tokens.push_back(bindingToken);
    }

    std::string merged;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i) {
            merged += ",";
        }
        merged += tokens[i];
    }

    targetNodeConfig->parameters.set(targetInput, merged);
    if (liveTarget != nullptr) {
        if (!liveTarget->setParameter(targetInput, merged, true)) {
            return false;
        }
    }

    return true;
}

static bool removeEdgeBindingLocked(GraphConfig& graph, IPipeline* pipeline, const EdgeConfig& edge)
{
    NodeConfig* targetNodeConfig = graph.findNode(edge.toNode);
    if (targetNodeConfig == nullptr) {
        return false;
    }

    const std::string sourcePort = (edge.fromPort.empty() || edge.fromPort == "output") ? "image" : edge.fromPort;
    const std::string targetInput = (edge.toPort.empty() || edge.toPort == "input") ? "image" : edge.toPort;
    const std::string bindingToken = edge.fromNode + "." + sourcePort;

    const std::string currentBinding = targetNodeConfig->parameters.stringValue(targetInput, std::string());
    std::vector<std::string> tokens = splitCommaValues(currentBinding);
    tokens.erase(std::remove(tokens.begin(), tokens.end(), bindingToken), tokens.end());

    std::string merged;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i) {
            merged += ",";
        }
        merged += tokens[i];
    }

    targetNodeConfig->parameters.set(targetInput, merged);
    Node* liveTarget = pipeline ? pipeline->findNode(edge.toNode) : nullptr;
    if (liveTarget != nullptr) {
        if (!liveTarget->setParameter(targetInput, merged, true)) {
            return false;
        }
    }

    return true;
}

void RuntimeController::setPipeline(std::unique_ptr<IPipeline> pipeline, const GraphConfig& graph)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    pipeline->setNodeExecutionCallback([this](const std::string& nodeId, const FrameContext& context) { notifyFrameContextObservers(nodeId, context); });
    m_pipeline = std::move(pipeline);
    m_graph = graph;
}

void RuntimeController::setPipelineRebuildHandler(const PipelineRebuildHandler& handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pipelineRebuildHandler = handler;
}

void RuntimeController::setIncrementalNodeFactory(const IncrementalNodeFactory& factory)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_incrementalNodeFactory = factory;
}

bool RuntimeController::setParameter(const std::string& nodeId, const std::string& parameterName, const ParameterValue& value, std::string* errorMessage)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_pipeline) {
        if (errorMessage != nullptr) {
            *errorMessage = "runtime pipeline is not available";
        }
        return false;
    }
    Node* node = m_pipeline->findNode(nodeId);
    if (node == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "node not found";
        }
        return false;
    }
    const bool allowLocked = m_pipeline != nullptr && m_pipeline->isStopped();
    if (!node->setParameter(parameterName, value, allowLocked, errorMessage)) {
        return false;
    }
    if (auto* config = m_graph.findNode(nodeId)) {
        config->parameters.set(parameterName, value);
    }
    return true;
}

bool RuntimeController::setParameterFromString(const std::string& nodeId, const std::string& parameterName, const std::string& value, std::string* errorMessage)
{
    std::string clean = value;
    while (!clean.empty() && (clean.front() == ' ' || clean.front() == '"' || clean.front() == '\n' || clean.front() == '\r')) {
        clean.erase(clean.begin());
    }
    while (!clean.empty() && (clean.back() == ' ' || clean.back() == '"' || clean.back() == '\n' || clean.back() == '\r')) {
        clean.pop_back();
    }
    if (clean == "true") {
        return setParameter(nodeId, parameterName, true, errorMessage);
    }
    if (clean == "false") {
        return setParameter(nodeId, parameterName, false, errorMessage);
    }
    try {
        if (clean.find('.') != std::string::npos) {
            return setParameter(nodeId, parameterName, std::stod(clean), errorMessage);
        }
        return setParameter(nodeId, parameterName, static_cast<int64_t>(std::stoll(clean)), errorMessage);
    } catch (...) {
        return setParameter(nodeId, parameterName, clean, errorMessage);
    }
}

bool RuntimeController::replaceGraph(const GraphConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_pipelineRebuildHandler) {
        if (m_pipeline && !m_pipeline->isStopped()) {
            return false;
        }

        std::unique_ptr<IPipeline> rebuilt = m_pipelineRebuildHandler(config, true);
        if (!rebuilt) {
            return false;
        }

        rebuilt->setNodeExecutionCallback([this](const std::string& nodeId, const FrameContext& context) { notifyFrameContextObservers(nodeId, context); });
        rebuilt->setStopped(true);
        rebuilt->run(1);

        if (m_pipeline) {
            m_pipeline->stop();
            m_pipeline->shutdown();
        }

        m_pipeline = std::move(rebuilt);
        m_graph = config;
        return true;
    }

    m_graph = config;
    return true;
}

bool RuntimeController::addNode(NodeConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_pipeline || !m_incrementalNodeFactory || !m_pipeline->isStopped()) {
        return false;
    }

    if (config.id.empty() || config.id == "__auto__") {
        config.id = generateUniqueNodeId(m_graph, config.type);
    } else {
        for (const auto& existing : m_graph.nodes()) {
            if (existing.id == config.id) {
                return false;
            }
        }
    }

    NodePtr node = m_incrementalNodeFactory(config);
    if (!node) {
        return false;
    }
    if (!node->init()) {
        return false;
    }

    m_pipeline->addNode(std::move(node));
    m_graph.addNode(config);
    return true;
}

bool RuntimeController::addEdge(const EdgeConfig& edge)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_pipeline || !m_pipeline->isStopped()) {
        return false;
    }

    bool fromFound = false;
    bool toFound = false;
    for (const auto& node : m_graph.nodes()) {
        if (node.id == edge.fromNode) {
            fromFound = true;
        }
        if (node.id == edge.toNode) {
            toFound = true;
        }
    }
    if (!fromFound || !toFound) {
        return false;
    }

    EdgeConfig normalizedEdge = edge;
    if (normalizedEdge.fromPort.empty() || normalizedEdge.fromPort == "output") {
        normalizedEdge.fromPort = "image";
    }
    if (normalizedEdge.toPort.empty() || normalizedEdge.toPort == "input") {
        normalizedEdge.toPort = "image";
    }

    for (const auto& existing : m_graph.edges()) {
        if (existing.fromNode == normalizedEdge.fromNode && existing.fromPort == normalizedEdge.fromPort && existing.toNode == normalizedEdge.toNode && existing.toPort == normalizedEdge.toPort) {
            return false;
        }
    }

    if (!applyEdgeBindingLocked(m_graph, m_pipeline.get(), normalizedEdge)) {
        return false;
    }

    m_pipeline->addEdge(normalizedEdge);
    m_graph.addEdge(normalizedEdge);
    return true;
}

bool RuntimeController::removeNode(const std::string& nodeId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_pipeline || !m_pipeline->isStopped()) {
        return false;
    }

    if (!m_graph.findNode(nodeId)) {
        return false;
    }

    if (!m_pipeline->removeNode(nodeId)) {
        return false;
    }

    return m_graph.removeNode(nodeId);
}

bool RuntimeController::renameNode(const std::string& nodeId, const std::string& newNodeId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_pipeline || !m_pipeline->isStopped() || newNodeId.empty() || newNodeId == "__auto__") {
        return false;
    }

    NodeConfig* nodeConfig = m_graph.findNode(nodeId);
    if (nodeConfig == nullptr || m_graph.findNode(newNodeId) != nullptr) {
        return false;
    }

    for (const auto& edge : m_graph.edges()) {
        if (edge.fromNode != nodeId) {
            continue;
        }

        NodeConfig* targetConfig = m_graph.findNode(edge.toNode);
        if (targetConfig == nullptr) {
            return false;
        }
        const std::string sourcePort = (edge.fromPort.empty() || edge.fromPort == "output") ? "image" : edge.fromPort;
        const std::string targetInput = (edge.toPort.empty() || edge.toPort == "input") ? "image" : edge.toPort;
        const std::string oldBinding = nodeId + "." + sourcePort;
        const std::string newBinding = newNodeId + "." + sourcePort;
        std::vector<std::string> bindings = splitCommaValues(targetConfig->parameters.stringValue(targetInput, std::string()));
        bool changed = false;
        for (auto& binding : bindings) {
            if (binding == oldBinding) {
                binding = newBinding;
                changed = true;
            }
        }
        if (!changed) {
            continue;
        }

        std::string merged;
        for (size_t i = 0; i < bindings.size(); ++i) {
            if (i) {
                merged += ",";
            }
            merged += bindings[i];
        }
        targetConfig->parameters.set(targetInput, merged);
        Node* liveTarget = m_pipeline->findNode(edge.toNode);
        if (liveTarget != nullptr && !liveTarget->setParameter(targetInput, merged, true)) {
            return false;
        }
    }

    if (!m_pipeline->renameNode(nodeId, newNodeId)) {
        return false;
    }

    nodeConfig->id = newNodeId;
    for (auto& edge : m_graph.edges()) {
        if (edge.fromNode == nodeId) {
            edge.fromNode = newNodeId;
        }
        if (edge.toNode == nodeId) {
            edge.toNode = newNodeId;
        }
    }
    return true;
}

bool RuntimeController::removeEdge(const EdgeConfig& edge)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_pipeline || !m_pipeline->isStopped()) {
        return false;
    }

    EdgeConfig normalizedEdge = edge;
    if (normalizedEdge.fromPort.empty() || normalizedEdge.fromPort == "output") {
        normalizedEdge.fromPort = "image";
    }
    if (normalizedEdge.toPort.empty() || normalizedEdge.toPort == "input") {
        normalizedEdge.toPort = "image";
    }

    auto edges = m_graph.edges();
    const auto beforeSize = edges.size();
    edges.erase(std::remove_if(edges.begin(), edges.end(),
                               [&](const EdgeConfig& existing) {
                                   return existing.fromNode == normalizedEdge.fromNode && existing.fromPort == normalizedEdge.fromPort && existing.toNode == normalizedEdge.toNode &&
                                          existing.toPort == normalizedEdge.toPort;
                               }),
                edges.end());
    if (edges.size() == beforeSize) {
        return false;
    }

    GraphConfig nextGraph = m_graph;
    nextGraph.edges() = std::move(edges);
    if (!removeEdgeBindingLocked(nextGraph, nullptr, normalizedEdge)) {
        return false;
    }

    if (m_pipelineRebuildHandler) {
        std::unique_ptr<IPipeline> rebuilt = m_pipelineRebuildHandler(nextGraph, true);
        if (!rebuilt) {
            return false;
        }

        rebuilt->setNodeExecutionCallback([this](const std::string& nodeId, const FrameContext& context) { notifyFrameContextObservers(nodeId, context); });
        rebuilt->setStopped(true);
        rebuilt->run(1);

        m_pipeline->stop();
        m_pipeline->shutdown();
        m_pipeline = std::move(rebuilt);
        m_graph = std::move(nextGraph);
        return true;
    }

    return false;
}

std::string RuntimeController::graphJson() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream json;
    json << "{\"nodes\":[";
    for (size_t i = 0; i < m_graph.nodes().size(); ++i) {
        const auto& node = m_graph.nodes()[i];
        if (i) {
            json << ",";
        }
        json << "{\"id\":\"" << jsonEscapeRuntime(node.id) << "\",\"type\":\"" << jsonEscapeRuntime(node.type) << "\"";
        json << ",\"parameters\":{";
        size_t index = 0;
        for (const auto& parameter : node.parameters.values()) {
            if (index++) {
                json << ",";
            }
            json << "\"" << jsonEscapeRuntime(parameter.first) << "\":" << valueToJson(parameter.second);
        }
        json << "}}";
    }
    json << "],\"edges\":[";
    for (size_t i = 0; i < m_graph.edges().size(); ++i) {
        const auto& edge = m_graph.edges()[i];
        if (i) {
            json << ",";
        }
        json << "\"";
        json << jsonEscapeRuntime(edge.fromNode);
        if (!edge.fromPort.empty()) {
            json << "." << jsonEscapeRuntime(edge.fromPort);
        }
        json << " -> " << jsonEscapeRuntime(edge.toNode);
        if (!edge.toPort.empty()) {
            json << "." << jsonEscapeRuntime(edge.toPort);
        }
        json << "\"";
    }
    json << "]}";
    return json.str();
}

std::string RuntimeController::nodeParametersJson(const std::string& nodeId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const NodeConfig* node = nullptr;
    for (const auto& item : m_graph.nodes()) {
        if (item.id == nodeId) {
            node = &item;
        }
    }
    if (node == nullptr) {
        return "{}";
    }
    std::ostringstream json;
    json << "{";
    size_t index = 0;
    for (const auto& parameter : node->parameters.values()) {
        if (index++) {
            json << ",";
        }
        json << "\"" << jsonEscapeRuntime(parameter.first) << "\":" << valueToJson(parameter.second);
    }
    json << "}";
    return json.str();
}

bool RuntimeController::nodeConfig(const std::string& nodeId, NodeConfig& config) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& item : m_graph.nodes()) {
        if (item.id == nodeId) {
            config = item;
            return true;
        }
    }
    return false;
}

bool RuntimeController::nodeState(const std::string& nodeId, NodeSchema& schema, ParameterSet& parameters) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_pipeline) {
        return false;
    }
    Node* node = m_pipeline->findNode(nodeId);
    if (node == nullptr) {
        return false;
    }
    schema = node->schema();
    parameters = node->currentParameters();
    return true;
}

bool RuntimeController::setStopped(bool stopped)
{
    IPipeline* pipeline = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        pipeline = m_pipeline.get();
    }

    if (pipeline == nullptr) {
        return false;
    }

    // Do not hold RuntimeController::m_mutex while stopping/starting nodes.
    // A stop request must be able to interrupt an in-flight run loop immediately.
    pipeline->setStopped(stopped);
    return true;
}

bool RuntimeController::isStopped() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_pipeline) {
        return false;
    }
    return m_pipeline->isStopped();
}

bool RuntimeController::pipelineStopped() const
{
    return isStopped();
}

int RuntimeController::runFrames(int maxFrames)
{
    IPipeline* pipeline = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        pipeline = m_pipeline.get();
    }

    if (pipeline == nullptr) {
        return 0;
    }

    // Run without holding RuntimeController::m_mutex so control calls (setStopped)
    // are not blocked by a long frame execution.
    return pipeline->run(maxFrames);
}

void RuntimeController::addFrameContextObserver(const FrameContextObserver& observer)
{
    std::lock_guard<std::mutex> lock(m_observerMutex);
    m_frameContextObservers.push_back(observer);
}

void RuntimeController::notifyFrameContextObservers(const std::string& nodeId, const FrameContext& context)
{
    std::vector<FrameContextObserver> observers;
    {
        std::lock_guard<std::mutex> lock(m_observerMutex);
        observers = m_frameContextObservers;
    }
    for (const auto& observer : observers) {
        if (observer) {
            observer(nodeId, context);
        }
    }
}
