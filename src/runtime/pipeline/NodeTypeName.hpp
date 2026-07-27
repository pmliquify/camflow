// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include <string>

/**
 * @brief Normalises a node type name to its canonical lowercase registration key.
 *
 * Converts @p typeName to all-lowercase so that the CLI parser and the @ref NodeFactory
 * registry use a consistent naming convention regardless of how the user specifies the
 * type in a pipeline expression or JSON graph.
 *
 * Example: `"V4L2Src"` → `"v4l2src"`
 *
 * @param typeName Raw type name string as provided by the user or parser.
 * @return Lowercase normalised type name.
 */
std::string normalizeNodeTypeName(const std::string& typeName);

/**
 * @brief Generates an automatic node identifier for a given type and index.
 *
 * Used by @ref CLIPipelineParser when a pipeline expression does not specify explicit
 * node identifiers. The generated identifier has the form `<type><index>`, e.g.
 * `"v4l2src0"`, `"sink0"`, `"probe1"`.
 *
 * @param typeName  The (optionally normalised) type name string.
 * @param index     Zero-based occurrence index of this type in the pipeline.
 * @return Auto-generated identifier string.
 */
std::string makeAutomaticNodeId(const std::string& typeName, size_t index);
