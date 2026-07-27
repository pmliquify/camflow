// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "parameters/Parameter.hpp"

#include <map>
#include <string>

/**
 * @brief A typed key-value container for node configuration parameters.
 *
 * ParameterSet is the runtime representation of the parameters parsed from a
 * pipeline expression or JSON graph and passed to @ref Node::configure. It stores
 * @ref ParameterValue variants indexed by parameter name and provides
 * convenience accessors that return typed values with a fallback default.
 *
 * ### Usage
 * @code
 * ParameterSet params;
 * params.set("device", std::string("/dev/video0"));
 * params.set("width",  int64_t(1920));
 *
 * std::string device = params.stringValue("device", "/dev/video0");
 * int width = static_cast<int>(params.intValue("width", 1280));
 * @endcode
 *
 * @see ParameterValue
 * @see ParameterInfo
 * @see Node::configure
 */
class ParameterSet
{
public:
    /**
     * @brief Stores or replaces the value under the given parameter name.
     * @param name  Parameter name.
     * @param value New value; replaces any previously stored value for @p name.
     */
    void set(const std::string& name, const ParameterValue& value);

    /**
     * @brief Returns @c true if a value has been stored for @p name.
     * @param name Parameter name to check.
     * @return @c true if the parameter is present.
     */
    bool contains(const std::string& name) const;

    /**
     * @brief Returns a pointer to the raw @ref ParameterValue for @p name.
     *
     * @param name Parameter name to look up.
     * @return Pointer to the value, or @c nullptr if the parameter is not set.
     */
    const ParameterValue* value(const std::string& name) const;

    /**
     * @brief Returns the string representation of a parameter's value.
     *
     * If the parameter exists and holds a `std::string` variant its value is returned
     * directly. Otherwise the variant is converted to string via @ref parameterValueToString.
     *
     * @param name          Parameter name.
     * @param defaultValue  Returned when the parameter is not set.
     * @return String value or @p defaultValue.
     */
    std::string stringValue(const std::string& name, const std::string& defaultValue = std::string()) const;

    /**
     * @brief Returns the integer value of a parameter.
     * @param name          Parameter name.
     * @param defaultValue  Returned when the parameter is not set or not an integer.
     * @return Integer value or @p defaultValue.
     */
    int64_t intValue(const std::string& name, int64_t defaultValue = 0) const;

    /**
     * @brief Returns the double value of a parameter.
     * @param name          Parameter name.
     * @param defaultValue  Returned when the parameter is not set or not a double.
     * @return Double value or @p defaultValue.
     */
    double doubleValue(const std::string& name, double defaultValue = 0.0) const;

    /**
     * @brief Returns the boolean value of a parameter.
     * @param name          Parameter name.
     * @param defaultValue  Returned when the parameter is not set or not a bool.
     * @return Boolean value or @p defaultValue.
     */
    bool boolValue(const std::string& name, bool defaultValue = false) const;

    /**
     * @brief Returns the complete map of all stored name-value pairs.
     * @return Read-only reference to the internal parameter map.
     */
    const std::map<std::string, ParameterValue>& values() const;

private:
    std::map<std::string, ParameterValue> m_values; ///< All stored parameter values.
};
