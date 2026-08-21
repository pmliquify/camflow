// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <utility>
#include <variant>
#include <vector>

/**
 * @brief Enumeration of the supported parameter value types.
 *
 * ParameterType identifies how a parameter value should be interpreted,
 * displayed and validated. It is stored in @ref ParameterInfo and used by
 * the REST API, the CLI help system and the parsers.
 */
enum class ParameterType
{
    Int,    ///< 64-bit signed integer value.
    Double, ///< Double-precision floating-point value.
    Bool,   ///< Boolean flag (true/false).
    String, ///< Arbitrary UTF-8 string.
    Option, ///< One of a predefined set of string options (see @ref ParameterInfo::options).
    Button  ///< Stateless trigger-style action parameter.
};

/**
 * @brief A type-safe variant holding any supported parameter value.
 *
 * ParameterValue is a `std::variant` that can store an `int64_t`, `double`, `bool`
 * or `std::string`. The active alternative should match the @ref ParameterType
 * declared in the corresponding @ref ParameterInfo.
 */
using ParameterValue = std::variant<int64_t, double, bool, std::string>;

/**
 * @brief Metadata descriptor for a single configurable parameter of a @ref Node.
 *
 * ParameterInfo describes everything the system needs to know about a parameter:
 * its name, type, documentation, default and boundary values, and whether it
 * can be modified at runtime. The collection of @ref ParameterInfo for a node
 * type constitutes its @ref ParameterSchema.
 *
 * This struct is used by:
 * - The CLI `--help` output to list available parameters.
 * - The REST API `/nodes/{id}/parameters` endpoint.
 * - The @ref ParameterSet validation logic.
 */
struct ParameterInfo
{
    ParameterInfo() = default;

    ParameterInfo(std::string name, ParameterType type, std::string description, ParameterValue defaultValue, ParameterValue minimumValue, ParameterValue maximumValue,
                  std::vector<std::string> options, bool runtimeWritable, std::vector<std::string> optionLabels = {}, std::string origin = {}, std::string source = {}, std::string group = {},
                  std::string groupDescription = {}, bool hasSideEffects = false, bool multiSelect = false) :
        name(std::move(name)),
        type(type),
        description(std::move(description)),
        defaultValue(std::move(defaultValue)),
        minimumValue(std::move(minimumValue)),
        maximumValue(std::move(maximumValue)),
        options(std::move(options)),
        runtimeWritable(runtimeWritable),
        optionLabels(std::move(optionLabels)),
        origin(std::move(origin)),
        source(std::move(source)),
        group(std::move(group)),
        groupDescription(std::move(groupDescription)),
        hasSideEffects(hasSideEffects),
        multiSelect(multiSelect)
    {
    }

    std::string name;                      ///< Parameter name as used in pipeline expressions and the REST API.
    ParameterType type;                    ///< Data type of the parameter.
    std::string description;               ///< Human-readable description of the parameter's purpose.
    ParameterValue defaultValue;           ///< Default value when the parameter is not explicitly configured.
    ParameterValue minimumValue;           ///< Minimum acceptable value (for Int and Double types).
    ParameterValue maximumValue;           ///< Maximum acceptable value (for Int and Double types).
    std::vector<std::string> options;      ///< Valid options for @ref ParameterType::Option parameters.
    bool runtimeWritable = true;           ///< @c true if the parameter may be updated while the runtime is running.
    bool readOnly = false;                 ///< @c true if the parameter can never be changed.
    std::vector<std::string> optionLabels; ///< Human-readable labels for @ref options.
    std::string origin;                    ///< Optional origin tag such as "v4l2".
    std::string source;                    ///< Optional source description such as a device path.
    std::string group;                     ///< Optional UI grouping label.
    std::string groupDescription;          ///< Optional UI group header text.
    bool hasSideEffects = false;           ///< @c true when changing this parameter requires refreshing dependent parameters.
    bool multiSelect = false;              ///< @c true when @ref ParameterType::Option should be rendered as multi-select.
};

/// Ordered list of @ref ParameterInfo descriptors for one node type.
using ParameterSchema = std::vector<ParameterInfo>;

/**
 * @brief Descriptor for one node input.
 */
struct NodeInputInfo
{
    NodeInputInfo() = default;

    NodeInputInfo(std::string name, std::string dataType, std::string description, bool allowMultipleBindings = false) :
        name(std::move(name)),
        dataType(std::move(dataType)),
        description(std::move(description)),
        allowMultipleBindings(allowMultipleBindings)
    {
    }

    std::string name;           ///< Input name used in CLI bindings and REST API.
    std::string dataType;       ///< Logical input data type (for example `image`, `int`).
    std::string description;    ///< Human-readable input description.
    bool allowMultipleBindings; ///< @c true when one input can be bound to multiple outputs.
};

using InputSchema = std::vector<NodeInputInfo>;

/**
 * @brief Descriptor for one node output.
 */
struct NodeOutputInfo
{
    NodeOutputInfo() = default;

    NodeOutputInfo(std::string name, std::string dataType, std::string description) :
        name(std::move(name)),
        dataType(std::move(dataType)),
        description(std::move(description))
    {
    }

    std::string name;        ///< Output name used in bindings (`nodeId.output`).
    std::string dataType;    ///< Logical output data type (for example `image`, `int`).
    std::string description; ///< Human-readable output description.
};

using OutputSchema = std::vector<NodeOutputInfo>;

/**
 * @brief Full node schema with separated parameters, inputs and outputs.
 */
struct NodeSchema
{
    ParameterSchema parameters;
    InputSchema inputs;
    OutputSchema outputs;
};

/**
 * @brief Converts a @ref ParameterValue to its string representation.
 * @param value The value to convert.
 * @return String representation (e.g. `"42"`, `"3.14"`, `"true"`, `"hello"`).
 */
std::string parameterValueToString(const ParameterValue& value);

/**
 * @brief Parses a raw string into a typed @ref ParameterValue.
 *
 * @param type  Target type for the conversion.
 * @param value Raw string to parse.
 * @return Parsed @ref ParameterValue. If @p value cannot be parsed as @p type the
 *         behaviour is implementation-defined (typically returns the default value).
 */
ParameterValue parameterValueFromString(ParameterType type, const std::string& value);
