// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "PipelineSimple.hpp"

#include "core/Logger.hpp"
#include "network/FrameContext.hpp"

#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <vector>

namespace
{
const std::string kTotalProfileId = "__TOTAL__";

std::string nodeLabel(const Node& node)
{
    return node.id() + " (" + node.typeName() + ")";
}

std::unordered_map<std::string, size_t> buildScopeConsumerBudget(const std::vector<EdgeConfig>& edges)
{
    std::unordered_map<std::string, size_t> budget;
    for (const auto& edge : edges) {
        if (edge.fromNode.empty() || edge.toNode.empty()) {
            continue;
        }
        budget[edge.fromNode] += 1;
    }
    return budget;
}

std::vector<std::string> consumeScopesForNode(const std::string& nodeId, const std::vector<EdgeConfig>& edges, std::unordered_map<std::string, size_t>& budget)
{
    std::vector<std::string> expired;
    for (const auto& edge : edges) {
        if (edge.toNode != nodeId || edge.fromNode.empty()) {
            continue;
        }

        auto it = budget.find(edge.fromNode);
        if (it == budget.end() || it->second == 0) {
            continue;
        }

        it->second -= 1;
        if (it->second == 0) {
            expired.push_back(edge.fromNode);
        }
    }
    return expired;
}
} // namespace

void Pipeline::addNode(NodePtr node)
{
    m_nodes.push_back(std::move(node));
}

bool Pipeline::removeNode(const std::string& nodeId)
{
    const auto nodeIt = std::find_if(m_nodes.begin(), m_nodes.end(), [&](const NodePtr& node) { return node && node->id() == nodeId; });
    if (nodeIt == m_nodes.end()) {
        return false;
    }

    (*nodeIt)->shutdown();
    m_nodes.erase(nodeIt);
    m_scopeEdges.erase(std::remove_if(m_scopeEdges.begin(), m_scopeEdges.end(), [&](const EdgeConfig& edge) { return edge.fromNode == nodeId || edge.toNode == nodeId; }), m_scopeEdges.end());
    return true;
}

bool Pipeline::renameNode(const std::string& nodeId, const std::string& newNodeId)
{
    if (newNodeId.empty() || findNode(newNodeId) != nullptr) {
        return false;
    }

    Node* node = findNode(nodeId);
    if (node == nullptr) {
        return false;
    }

    node->setId(newNodeId);
    for (auto& edge : m_scopeEdges) {
        if (edge.fromNode == nodeId) {
            edge.fromNode = newNodeId;
        }
        if (edge.toNode == nodeId) {
            edge.toNode = newNodeId;
        }
    }
    return true;
}

void Pipeline::addEdge(const EdgeConfig& edge)
{
    m_scopeEdges.push_back(edge);
}

void Pipeline::setProfilingEnabled(bool enabled)
{
    m_profiler.setEnabled(enabled);
}

void Pipeline::setStopped(bool stopped)
{
    const bool wasStopped = m_stopped.exchange(stopped);
    if (wasStopped == stopped) {
        return;
    }

    LOG_INFO(std::string("Pipeline state: ") + (wasStopped ? "stopped" : "running") + " -> " + (stopped ? "stopped" : "running"));

    // Keep stop/start as a fast state toggle so REST stop requests never block on
    // potentially long device driver calls while a frame is in flight.
}

bool Pipeline::isStopped() const
{
    return m_stopped.load();
}

void Pipeline::setNodeExecutionCallback(const std::function<void(const std::string&, const FrameContext&)>& callback)
{
    m_nodeExecutionCallback = callback;
}

Node* Pipeline::findNode(const std::string& id)
{
    for (auto& node : m_nodes) {
        if (node->id() == id) {
            return node.get();
        }
    }
    return nullptr;
}

bool Pipeline::init()
{
    for (auto& node : m_nodes) {
        LOG_INFO("Node init: " + nodeLabel(*node));
        if (!node->init()) {
            LOG_ERROR("Failed to initialize node: " + node->id());
            return false;
        }
        LOG_INFO("Node init done: " + nodeLabel(*node));
    }
    return true;
}

bool Pipeline::start()
{
    for (auto& node : m_nodes) {
        LOG_INFO("Node start: " + nodeLabel(*node));
        if (!node->start()) {
            LOG_ERROR("Failed to start node: " + node->id());
            return false;
        }
        LOG_INFO("Node start done: " + nodeLabel(*node));
    }
    return true;
}

int Pipeline::run(int maxFrames)
{
    m_profiler.clear();

    int frames = 0;
    while (maxFrames <= 0 || frames < maxFrames) {
        if (m_stopped.load()) {
            if (!m_nodesStopped.exchange(true)) {
                std::lock_guard<std::mutex> lock(m_lifecycleMutex);
                stop();
            }
            if (maxFrames > 0) {
                break;
            }
            waitWhileStopped();
            continue;
        }

        if (m_nodesStopped.exchange(false)) {
            std::lock_guard<std::mutex> lock(m_lifecycleMutex);
            if (!start()) {
                LOG_ERROR("Failed to restart nodes after stop state was cleared");
                m_stopped.store(true);
                m_nodesStopped.store(true);
                if (maxFrames > 0) {
                    break;
                }
                waitWhileStopped();
                continue;
            }
        }
        m_profiler.start(kTotalProfileId);

        FrameContext context;
        auto remainingScopeConsumers = buildScopeConsumerBudget(m_scopeEdges);

        bool frameOk = true;
        for (auto& node : m_nodes) {
            const bool ok = runNodeWithProfiling(node->id(), context);

            if (!ok) {
                frameOk = false;
                break;
            }

            const auto expiredScopes = consumeScopesForNode(node->id(), m_scopeEdges, remainingScopeConsumers);
            for (const auto& scope : expiredScopes) {
                context.eraseScope(scope);
            }
        }

        if (!frameOk) {
            if (m_stopped.load()) {
                m_profiler.record(kTotalProfileId, false);
                if (maxFrames > 0) {
                    break;
                }
                continue;
            }
            m_profiler.record(kTotalProfileId, false);
            break;
        }

        m_profiler.record(kTotalProfileId, true);
        ++frames;
    }

    m_profiler.logReport(nodeOrder());
    return frames;
}

void Pipeline::waitWhileStopped()
{
    while (m_stopped.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
}

bool Pipeline::runNodeWithProfiling(const std::string& nodeId, FrameContext& context)
{
    Node* node = findNode(nodeId);
    if (node == nullptr) {
        return false;
    }

    context.setWriteScope(nodeId);
    const std::string inferred = inferredInputScope(nodeId);
    if (inferred.empty()) {
        context.setReadScopes({});
    } else {
        context.setReadScopes({inferred});
    }

    m_profiler.start(nodeId);
    const bool ok = node->process(context);
    m_profiler.record(nodeId, ok);
    if (ok && m_nodeExecutionCallback) {
        m_nodeExecutionCallback(nodeId, context);
    }
    return ok;
}

std::string Pipeline::inferredInputScope(const std::string& nodeId) const
{
    std::string inferred;
    bool ambiguous = false;

    for (const auto& edge : m_scopeEdges) {
        if (edge.toNode != nodeId) {
            continue;
        }
        if (inferred.empty()) {
            inferred = edge.fromNode;
            continue;
        }
        if (inferred != edge.fromNode) {
            ambiguous = true;
            break;
        }
    }

    if (!ambiguous && !inferred.empty()) {
        return inferred;
    }

    return previousNodeId(nodeId);
}

std::string Pipeline::previousNodeId(const std::string& nodeId) const
{
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        if (m_nodes[i]->id() != nodeId) {
            continue;
        }
        if (i == 0) {
            return std::string();
        }
        return m_nodes[i - 1]->id();
    }
    return std::string();
}

void Pipeline::shutdown()
{
    for (auto it = m_nodes.rbegin(); it != m_nodes.rend(); ++it) {
        LOG_INFO("Node shutdown: " + nodeLabel(*(*it)));
        (*it)->shutdown();
        LOG_INFO("Node shutdown done: " + nodeLabel(*(*it)));
    }
}

void Pipeline::stop()
{
    for (auto it = m_nodes.rbegin(); it != m_nodes.rend(); ++it) {
        LOG_INFO("Node stop: " + nodeLabel(*(*it)));
        (*it)->stop();
        LOG_INFO("Node stop done: " + nodeLabel(*(*it)));
    }
}

std::vector<std::string> Pipeline::nodeOrder() const
{
    std::vector<std::string> ids;
    ids.reserve(m_nodes.size());
    for (const auto& node : m_nodes) {
        ids.push_back(node->id());
    }
    return ids;
}
