// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "pipeline/GraphConfig.hpp"

#include <string>

/**
 * @brief Parser that constructs a @ref GraphConfig from a JSON pipeline description.
 *
 * JsonPipelineParser reads the CamFlow JSON graph format and populates a @ref GraphConfig
 * with all node and edge configurations. Both file-based and in-memory text parsing
 * are supported.
 *
 * ### JSON format
 * The expected JSON structure is documented in `docs/release/graph_json.md`. At the top level
 * the document contains a `nodes` array and an optional `edges` array:
 * @code{.json}
 * {
 *   "nodes": [
 *     { "id": "cam", "type": "v4l2src", "parameters": { "device": "/dev/video0" } },
 *     { "id": "sink", "type": "tcpsink" }
 *   ],
 *   "edges": [
 *     { "from": "cam", "to": "sink" }
 *   ]
 * }
 * @endcode
 *
 * ### Error handling
 * Both methods fill @p errorMessage with a human-readable description on failure.
 * The @ref GraphConfig is left in an unspecified state when parsing fails.
 *
 * @see GraphConfig
 * @see CLIPipelineParser
 */
class JsonPipelineParser
{
public:
    /**
     * @brief Parses a JSON pipeline file and populates @p config.
     *
     * Opens the file at @p fileName, reads its full content and delegates to
     * @ref parseText. The file must be UTF-8 encoded and contain valid JSON.
     *
     * @param fileName      Path to the JSON file to parse.
     * @param config        Output parameter; receives all nodes and edges on success.
     * @param errorMessage  Output parameter; receives a descriptive error on failure.
     * @return @c true on success; @c false if the file could not be opened or parsed.
     */
    bool parseFile(const std::string& fileName, GraphConfig& config, std::string& errorMessage) const;

    /**
     * @brief Parses a JSON string and populates @p config.
     *
     * Parses the JSON text in @p text, creates @ref NodeConfig and @ref EdgeConfig
     * entries for each element and adds them to @p config.
     *
     * @param text          UTF-8 JSON string to parse.
     * @param config        Output parameter; receives all nodes and edges on success.
     * @param errorMessage  Output parameter; receives a descriptive error on failure.
     * @return @c true on success; @c false if the JSON is malformed or missing required fields.
     */
    bool parseText(const std::string& text, GraphConfig& config, std::string& errorMessage) const;

private:
    /**
     * @brief Converts a raw JSON string value to a typed @ref ParameterValue.
     *
     * Attempts to interpret @p value as int, double, bool or string in that order.
     *
     * @param value The raw string extracted from the JSON document.
     * @return Typed @ref ParameterValue.
     */
    ParameterValue parseValue(const std::string& value) const;
};
