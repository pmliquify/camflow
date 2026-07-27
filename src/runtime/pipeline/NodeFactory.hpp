// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "pipeline/Node.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Enumeration of the functional roles a pipeline node can occupy.
 *
 * NodeKind categorises registered node types into four distinct roles
 * that correspond to the data-flow position of the node within a pipeline.
 * The kind is stored in the @ref NodeFactory registry and exposed via
 * @ref NodeFactory::kind. It is used by the application's help system to
 * group nodes in the printed node list and by @ref CLIPipelineParser to
 * identify sink nodes.
 */
enum class NodeKind
{
    Source,    ///< Data-producing node (cameras, file readers, test generators).
    Processor, ///< Data-transforming node (processors, compositors).
    Sink,      ///< Data-consuming node (file writers, display sinks, network sinks).
    Probe      ///< Non-destructive inspection node (logs frame info, passes data through).
};

/// Owning smart-pointer type used throughout the pipeline for node instances.
typedef std::unique_ptr<Node> NodePtr;

/**
 * @brief Registry and factory for all available pipeline node types.
 *
 * NodeFactory maintains a map of type name strings to creator functions. Application
 * code registers all available node types at startup via @ref registerType; the
 * @ref PipelineBuilder then calls @ref create with the type name from a @ref NodeConfig
 * to instantiate nodes.
 *
 * ### Registration
 * @code
 * factory.registerType("v4l2src", NodeKind::Source, []() { return std::make_unique<V4L2Source>(); });
 * factory.registerType("tcpsink", NodeKind::Sink,   []() { return std::make_unique<TCPSink>(); });
 * @endcode
 *
 * ### Creation
 * @code
 * NodePtr node = factory.create("v4l2src");
 * if (!node) { }
 * @endcode
 *
 * ### Schema and description
 * The factory delegates schema and description queries to a freshly created
 * instance of the requested type. This is necessary because @ref Node::schema
 * and @ref Node::description are virtual methods rather than static members.
 *
 * @see Node
 * @see NodeKind
 * @see NodePtr
 */
class NodeFactory
{
public:
    /// Function type that creates a new node instance.
    using Creator = std::function<NodePtr()>;

    /**
     * @brief Registers a new node type under the given type name.
     *
     * The @p typeName is used as the key for all subsequent lookups. If a type
     * with the same name is registered again the previous entry is silently replaced.
     *
     * @param typeName  The unique string key for this node type (e.g. `"v4l2src"`).
     * @param kind      The functional role of the node (@ref NodeKind).
     * @param creator   A callable that returns a new node instance when invoked.
     */
    void registerType(const std::string& typeName, NodeKind kind, Creator creator);

    /**
     * @brief Creates a new node instance for the given type name.
     *
     * Calls the creator function registered under @p typeName and returns the
     * resulting owning pointer.
     *
     * @param typeName The type name to look up.
     * @return Owning pointer to the new node, or @c nullptr if @p typeName is not registered.
     */
    NodePtr create(const std::string& typeName) const;

    /**
     * @brief Returns the names of all registered node types.
     * @return Alphabetically sorted list of all registered type name strings.
     */
    std::vector<std::string> registeredTypes() const;

    /**
     * @brief Returns the names of all registered node types with the given kind.
     * @param kind Filter criterion.
     * @return List of type name strings matching @p kind.
     */
    std::vector<std::string> registeredTypes(NodeKind kind) const;

    /**
     * @brief Returns the parameter schema of the given node type.
     *
     * Creates a temporary instance of the type and delegates to @ref Node::schema.
     * Returns an empty schema if @p typeName is not registered.
     *
     * @param typeName The type name to query.
     * @return ParameterSchema describing the configurable parameters.
     */
    NodeSchema schema(const std::string& typeName) const;

    /**
     * @brief Returns the human-readable description of the given node type.
     *
     * Creates a temporary instance and delegates to @ref Node::description.
     * Returns an empty string if @p typeName is not registered.
     *
     * @param typeName The type name to query.
     * @return Description string.
     */
    std::string description(const std::string& typeName) const;

    /**
     * @brief Returns the @ref NodeKind of the given type name.
     *
     * @param typeName The type name to query.
     * @return The NodeKind registered for @p typeName, or @c NodeKind::Source as default
     *         if @p typeName is not found.
     */
    NodeKind kind(const std::string& typeName) const;

private:
    /// Internal entry combining role and creator for one registered type.
    struct Entry
    {
        NodeKind kind;   ///< Functional role of this node type.
        Creator creator; ///< Factory function that creates a new instance.
    };
    std::map<std::string, Entry> m_creators; ///< Registry: type name → Entry.
};
