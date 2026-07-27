// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "ParameterSet.hpp"

void ParameterSet::set(const std::string& name, const ParameterValue& value)
{
    m_values[name] = value;
}

bool ParameterSet::contains(const std::string& name) const
{
    return m_values.find(name) != m_values.end();
}

const ParameterValue* ParameterSet::value(const std::string& name) const
{
    auto it = m_values.find(name);
    if (it == m_values.end()) {
        return nullptr;
    }
    return &it->second;
}

std::string ParameterSet::stringValue(const std::string& name, const std::string& defaultValue) const
{
    const ParameterValue* currentValue = value(name);
    if (currentValue == nullptr) {
        return defaultValue;
    }
    return parameterValueToString(*currentValue);
}

int64_t ParameterSet::intValue(const std::string& name, int64_t defaultValue) const
{
    const ParameterValue* currentValue = value(name);
    if (currentValue == nullptr) {
        return defaultValue;
    }
    if (std::holds_alternative<int64_t>(*currentValue)) {
        return std::get<int64_t>(*currentValue);
    }
    return std::stoll(parameterValueToString(*currentValue));
}

double ParameterSet::doubleValue(const std::string& name, double defaultValue) const
{
    const ParameterValue* currentValue = value(name);
    if (currentValue == nullptr) {
        return defaultValue;
    }
    if (std::holds_alternative<double>(*currentValue)) {
        return std::get<double>(*currentValue);
    }
    if (std::holds_alternative<int64_t>(*currentValue)) {
        return static_cast<double>(std::get<int64_t>(*currentValue));
    }
    return std::stod(parameterValueToString(*currentValue));
}

bool ParameterSet::boolValue(const std::string& name, bool defaultValue) const
{
    const ParameterValue* currentValue = value(name);
    if (currentValue == nullptr) {
        return defaultValue;
    }
    if (std::holds_alternative<bool>(*currentValue)) {
        return std::get<bool>(*currentValue);
    }
    std::string text = parameterValueToString(*currentValue);
    return text == "true" || text == "1" || text == "yes" || text == "on";
}

const std::map<std::string, ParameterValue>& ParameterSet::values() const
{
    return m_values;
}
