// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "Node.hpp"

#include "core/Logger.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace
{

std::vector<std::string> splitCommaSeparated(const std::string& text)
{
    std::vector<std::string> result;
    std::string token;
    std::istringstream stream(text);
    while (std::getline(stream, token, ',')) {
        size_t begin = 0;
        while (begin < token.size() && std::isspace(static_cast<unsigned char>(token[begin]))) {
            ++begin;
        }
        size_t end = token.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(token[end - 1]))) {
            --end;
        }
        if (end > begin) {
            result.push_back(token.substr(begin, end - begin));
        }
    }
    return result;
}

} // namespace

static std::string toLowerCopy(const std::string& text)
{
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower;
}

Node::~Node() = default;

void Node::setId(const std::string& id)
{
    m_id = id;
}
const std::string& Node::id() const
{
    return m_id;
}
void Node::setImageConverter(IImageConverter* converter)
{
    m_converter = converter;
}

IImageConverter* Node::converter() const
{
    if (m_converter == nullptr) {
        LOG_ERROR("Node '" + m_id + "' requires an image converter");
    }
    return m_converter;
}
std::string Node::description() const
{
    return std::string();
}
NodeSchema Node::schema() const
{
    return NodeSchema{};
}
bool Node::configure(const ParameterSet& parameters)
{
    m_schemaByName.clear();
    m_inputSchemaByName.clear();
    m_inputBindings.clear();

    const NodeSchema currentSchema = schema();
    for (const auto& info : currentSchema.parameters) {
        m_schemaByName[info.name] = info;
    }
    for (const auto& info : currentSchema.inputs) {
        m_inputSchemaByName[info.name] = info;
    }

    ParameterSet configured;
    for (const auto& item : parameters.values()) {
        auto inputInfo = m_inputSchemaByName.find(item.first);
        if (inputInfo != m_inputSchemaByName.end()) {
            const std::string value = parameterValueToString(item.second);
            const auto parts = splitCommaSeparated(value);
            for (const auto& part : parts) {
                const size_t dotPos = part.find('.');
                if (dotPos == std::string::npos || dotPos == 0 || dotPos + 1 >= part.size()) {
                    LOG_WARNING("Ignoring invalid input binding '" + part + "' for " + id() + "." + item.first + " (expected <nodeId>.<output>)");
                    continue;
                }
                if (!inputInfo->second.allowMultipleBindings && !m_inputBindings[item.first].empty()) {
                    LOG_WARNING("Ignoring extra input binding '" + part + "' for single-binding input " + id() + "." + item.first);
                    continue;
                }
                m_inputBindings[item.first].push_back({part.substr(0, dotPos), part.substr(dotPos + 1)});
            }
            continue;
        }
        configured.set(item.first, item.second);
    }

    for (const auto& info : currentSchema.parameters) {
        if (!configured.contains(info.name)) {
            configured.set(info.name, info.defaultValue);
        }
    }

    m_parameters = configured;
    return true;
}
bool Node::setParameter(const std::string& name, const ParameterValue& value, bool allowLocked, std::string* errorMessage)
{
    auto inputIt = m_inputSchemaByName.find(name);
    if (inputIt != m_inputSchemaByName.end()) {
        if (!allowLocked) {
            return false;
        }

        const std::string textValue = parameterValueToString(value);
        const auto parsed = splitCommaSeparated(textValue);

        std::vector<std::pair<std::string, std::string>> bindings;
        for (const auto& part : parsed) {
            const size_t dotPos = part.find('.');
            if (dotPos == std::string::npos || dotPos == 0 || dotPos + 1 >= part.size()) {
                LOG_WARNING("Ignoring invalid input binding '" + part + "' for " + id() + "." + name + " (expected <nodeId>.<output>)");
                continue;
            }
            if (!inputIt->second.allowMultipleBindings && !bindings.empty()) {
                LOG_WARNING("Ignoring extra input binding '" + part + "' for single-binding input " + id() + "." + name);
                continue;
            }
            bindings.push_back({part.substr(0, dotPos), part.substr(dotPos + 1)});
        }

        m_inputBindings[name] = bindings;
        m_parameters.set(name, textValue);
        return true;
    }

    const ParameterInfo* info = parameterInfo(name);
    if (info != nullptr) {
        if (!info->runtimeWritable && !allowLocked) {
            if (errorMessage != nullptr) {
                *errorMessage = "parameter is not writable while runtime is running";
            }
            return false;
        }
    }

    const ParameterValue* existing = m_parameters.value(name);
    const bool alwaysTrigger = info != nullptr && info->type == ParameterType::Button;
    if (!alwaysTrigger && existing != nullptr && *existing == value) {
        return true;
    }

    ParameterValue previous;
    const ParameterValue* previousPtr = nullptr;
    if (existing != nullptr) {
        previous = *existing;
        previousPtr = &previous;
    }

    ParameterSet previousParameters = m_parameters;
    m_parameters.set(name, value);
    std::string callbackError;
    if (!onParameterChanged(name, value, previousPtr, callbackError)) {
        m_parameters = std::move(previousParameters);
        if (errorMessage != nullptr) {
            *errorMessage = callbackError.empty() ? "parameter value was rejected" : callbackError;
        }
        return false;
    }
    return true;
}
bool Node::init()
{
    return true;
}

bool Node::start()
{
    return true;
}

ParameterSet Node::currentParameters() const
{
    return m_parameters;
}

void Node::shutdown() {}

void Node::stop() {}

const ParameterSet& Node::runtimeParameters() const
{
    return m_parameters;
}

const ParameterInfo* Node::parameterInfo(const std::string& name) const
{
    auto it = m_schemaByName.find(name);
    if (it == m_schemaByName.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<std::pair<std::string, std::string>> Node::inputBindings(const std::string& name) const
{
    auto it = m_inputBindings.find(name);
    if (it == m_inputBindings.end()) {
        return {};
    }
    return it->second;
}

std::vector<std::pair<std::string, std::string>> Node::resolveInputBindings(const std::string& name, const FrameContext& context) const
{
    const auto explicitBindings = inputBindings(name);
    if (!explicitBindings.empty()) {
        return explicitBindings;
    }

    std::vector<std::pair<std::string, std::string>> resolved;
    for (const auto& scope : context.readScopes()) {
        if (context.contains(scope + "." + name)) {
            resolved.push_back({scope, name});
        }
    }
    if (!resolved.empty()) {
        return resolved;
    }

    if (context.contains(name)) {
        resolved.push_back({std::string(), name});
    }
    return resolved;
}

const ParameterValue* Node::parameter(const std::string& name) const
{
    return m_parameters.value(name);
}

bool Node::parameterBool(const std::string& name, bool fallback) const
{
    const ParameterValue* value = parameter(name);
    if (value == nullptr) {
        return fallback;
    }
    if (std::holds_alternative<bool>(*value)) {
        return std::get<bool>(*value);
    }
    if (std::holds_alternative<int64_t>(*value)) {
        return std::get<int64_t>(*value) != 0;
    }
    if (std::holds_alternative<double>(*value)) {
        return std::get<double>(*value) != 0.0;
    }
    const std::string text = toLowerCopy(std::get<std::string>(*value));
    return text == "1" || text == "true" || text == "yes" || text == "on";
}

int64_t Node::parameterInt(const std::string& name, int64_t fallback) const
{
    const ParameterValue* value = parameter(name);
    if (value == nullptr) {
        return fallback;
    }
    if (std::holds_alternative<int64_t>(*value)) {
        return std::get<int64_t>(*value);
    }
    if (std::holds_alternative<bool>(*value)) {
        return std::get<bool>(*value) ? 1 : 0;
    }
    if (std::holds_alternative<double>(*value)) {
        return static_cast<int64_t>(std::get<double>(*value));
    }
    try {
        return std::stoll(std::get<std::string>(*value));
    } catch (...) {
        return fallback;
    }
}

double Node::parameterDouble(const std::string& name, double fallback) const
{
    const ParameterValue* value = parameter(name);
    if (value == nullptr) {
        return fallback;
    }
    if (std::holds_alternative<double>(*value)) {
        return std::get<double>(*value);
    }
    if (std::holds_alternative<int64_t>(*value)) {
        return static_cast<double>(std::get<int64_t>(*value));
    }
    if (std::holds_alternative<bool>(*value)) {
        return std::get<bool>(*value) ? 1.0 : 0.0;
    }
    try {
        return std::stod(std::get<std::string>(*value));
    } catch (...) {
        return fallback;
    }
}

std::string Node::parameterString(const std::string& name, const std::string& fallback) const
{
    const ParameterValue* value = parameter(name);
    if (value == nullptr) {
        return fallback;
    }
    if (std::holds_alternative<std::string>(*value)) {
        return std::get<std::string>(*value);
    }
    return parameterValueToString(*value);
}

const ParameterSet& Node::configuredParameters() const
{
    return m_parameters;
}

bool Node::onParameterChanged(const std::string&, const ParameterValue&, const ParameterValue*, std::string&)
{
    return true;
}
