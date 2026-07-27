// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "Parameter.hpp"

#include <cstdlib>
#include <sstream>

std::string parameterValueToString(const ParameterValue& value)
{
    if (std::holds_alternative<int64_t>(value)) {
        return std::to_string(std::get<int64_t>(value));
    }
    if (std::holds_alternative<double>(value)) {
        std::ostringstream stream;
        stream << std::get<double>(value);
        return stream.str();
    }
    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? "true" : "false";
    }
    return std::get<std::string>(value);
}

ParameterValue parameterValueFromString(ParameterType type, const std::string& value)
{
    switch (type) {
    case ParameterType::Int:
    case ParameterType::Button:
        return static_cast<int64_t>(std::strtoll(value.c_str(), nullptr, 10));
    case ParameterType::Double:
        return std::strtod(value.c_str(), nullptr);
    case ParameterType::Bool:
        return value == "true" || value == "1" || value == "yes" || value == "on";
    case ParameterType::Option:
    case ParameterType::String:
    default:
        return value;
    }
}
