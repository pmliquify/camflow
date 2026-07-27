// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "FrameContext.hpp"

#include <algorithm>

void FrameContext::setWriteScope(const std::string& scope)
{
    m_writeScope = scope;
}

void FrameContext::setReadScopes(const std::vector<std::string>& scopes)
{
    m_readScopes = scopes;
}

void FrameContext::touchScope(const std::string& scope)
{
    auto existing = std::find(m_readScopes.begin(), m_readScopes.end(), scope);
    if (existing != m_readScopes.end()) {
        m_readScopes.erase(existing);
    }
    m_readScopes.push_back(scope);
}

const std::string& FrameContext::writeScope() const
{
    return m_writeScope;
}

const std::vector<std::string>& FrameContext::readScopes() const
{
    return m_readScopes;
}

bool FrameContext::contains(const std::string& name) const
{
    return resolveEntry(name) != nullptr;
}

const std::any* FrameContext::valueAny(const std::string& name) const
{
    const Entry* entry = resolveEntry(name);
    if (entry == nullptr || !entry->value) {
        return nullptr;
    }
    return entry->value.get();
}

const std::any* FrameContext::valueAny(const std::string& scope, const std::string& name) const
{
    const Entry* entry = resolveScopedEntry(scope, name);
    if (entry == nullptr || !entry->value) {
        return nullptr;
    }
    return entry->value.get();
}

std::vector<std::string> FrameContext::keys() const
{
    std::vector<std::string> result;
    for (const auto& scoped : m_scopes) {
        for (const auto& item : scoped.second) {
            result.push_back(qualifiedKey(scoped.first, item.first));
        }
    }
    return result;
}

void FrameContext::mergeFrom(const FrameContext& other)
{
    for (const auto& scope : other.m_readScopes) {
        touchScope(scope);
    }
    for (const auto& scoped : other.m_scopes) {
        touchScope(scoped.first);
        auto& targetScope = m_scopes[scoped.first];
        for (const auto& item : scoped.second) {
            targetScope[item.first] = item.second;
        }
    }
}

void FrameContext::clear()
{
    m_scopes.clear();
    m_readScopes.clear();
}

bool FrameContext::eraseScope(const std::string& scope)
{
    const auto removed = m_scopes.erase(scope);
    if (removed == 0) {
        return false;
    }

    m_readScopes.erase(std::remove(m_readScopes.begin(), m_readScopes.end(), scope), m_readScopes.end());
    return true;
}

FrameContext::QualifiedName FrameContext::splitQualifiedName(const std::string& name)
{
    const size_t pos = name.find('.');
    if (pos == std::string::npos) {
        return {std::string(), name};
    }
    return {name.substr(0, pos), name.substr(pos + 1)};
}

std::string FrameContext::qualifiedKey(const std::string& scope, const std::string& name)
{
    if (scope.empty()) {
        return name;
    }
    return scope + "." + name;
}

const FrameContext::Entry* FrameContext::resolveEntry(const std::string& name) const
{
    const QualifiedName requested = splitQualifiedName(name);
    if (!requested.scope.empty()) {
        return resolveScopedEntry(requested.scope, requested.key);
    }

    for (auto it = m_readScopes.rbegin(); it != m_readScopes.rend(); ++it) {
        const auto& scope = *it;
        if (const Entry* entry = resolveScopedEntry(scope, requested.key)) {
            return entry;
        }
    }

    return nullptr;
}

FrameContext::Entry* FrameContext::resolveEntry(const std::string& name)
{
    const Entry* entry = static_cast<const FrameContext*>(this)->resolveEntry(name);
    return const_cast<Entry*>(entry);
}

const FrameContext::Entry* FrameContext::resolveScopedEntry(const std::string& scope, const std::string& name) const
{
    auto scopeIt = m_scopes.find(scope);
    if (scopeIt == m_scopes.end()) {
        return nullptr;
    }
    auto keyIt = scopeIt->second.find(name);
    if (keyIt == scopeIt->second.end()) {
        return nullptr;
    }
    return &keyIt->second;
}

FrameContext::Entry* FrameContext::resolveScopedEntry(const std::string& scope, const std::string& name)
{
    return const_cast<Entry*>(static_cast<const FrameContext*>(this)->resolveScopedEntry(scope, name));
}
