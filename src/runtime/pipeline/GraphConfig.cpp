// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "GraphConfig.hpp"

#include <algorithm>

void GraphConfig::addNode(const NodeConfig& node)
{
    m_nodes.push_back(node);
}

void GraphConfig::addEdge(const EdgeConfig& edge)
{
    m_edges.push_back(edge);
}

NodeConfig* GraphConfig::findNode(const std::string& id)
{
    for (auto& node : m_nodes) {
        if (node.id == id) {
            return &node;
        }
    }
    return nullptr;
}

bool GraphConfig::removeNode(const std::string& id)
{
    const auto nodeIt = std::find_if(m_nodes.begin(), m_nodes.end(), [&](const NodeConfig& node) { return node.id == id; });
    if (nodeIt == m_nodes.end()) {
        return false;
    }

    m_nodes.erase(nodeIt);
    m_edges.erase(std::remove_if(m_edges.begin(), m_edges.end(), [&](const EdgeConfig& edge) { return edge.fromNode == id || edge.toNode == id; }), m_edges.end());
    return true;
}

std::vector<NodeConfig>& GraphConfig::nodes()
{
    return m_nodes;
}

const std::vector<NodeConfig>& GraphConfig::nodes() const
{
    return m_nodes;
}

std::vector<EdgeConfig>& GraphConfig::edges()
{
    return m_edges;
}

const std::vector<EdgeConfig>& GraphConfig::edges() const
{
    return m_edges;
}

bool GraphConfig::empty() const
{
    return m_nodes.empty();
}
