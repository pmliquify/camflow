// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "NodeFactory.hpp"

#include "NodeTypeName.hpp"

void NodeFactory::registerType(const std::string& typeName, NodeKind kind, Creator creator)
{
    m_creators[normalizeNodeTypeName(typeName)] = {kind, creator};
}

NodePtr NodeFactory::create(const std::string& typeName) const
{
    auto it = m_creators.find(normalizeNodeTypeName(typeName));
    if (it == m_creators.end()) {
        return nullptr;
    }
    return it->second.creator();
}

std::vector<std::string> NodeFactory::registeredTypes() const
{
    std::vector<std::string> result;
    for (const auto& item : m_creators) {
        result.push_back(item.first);
    }
    return result;
}

std::vector<std::string> NodeFactory::registeredTypes(NodeKind kind) const
{
    std::vector<std::string> result;
    for (const auto& item : m_creators) {
        if (item.second.kind == kind) {
            result.push_back(item.first);
        }
    }
    return result;
}

NodeSchema NodeFactory::schema(const std::string& typeName) const
{
    auto node = create(typeName);
    if (!node) {
        return NodeSchema{};
    }
    return node->schema();
}

std::string NodeFactory::description(const std::string& typeName) const
{
    auto node = create(typeName);
    if (!node) {
        return {};
    }
    return node->description();
}

NodeKind NodeFactory::kind(const std::string& typeName) const
{
    auto it = m_creators.find(normalizeNodeTypeName(typeName));
    if (it == m_creators.end()) {
        return NodeKind::Processor;
    }
    return it->second.kind;
}
