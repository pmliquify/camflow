// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "parameters/ParameterSet.hpp"

#include <string>
#include <vector>

/**
 * @brief Configuration data for a single node in the pipeline graph.
 *
 * NodeConfig is a plain data structure produced by parsers (@ref JsonPipelineParser,
 * @ref CLIPipelineParser) and consumed by @ref PipelineBuilder to instantiate and
 * configure nodes. It contains:
 * - A unique string identifier that labels the node within the graph.
 * - The node type name that is looked up in the @ref NodeFactory registry.
 * - A @ref ParameterSet with all key-value pairs specified in the graph description.
 */
struct NodeConfig
{
    std::string id;          ///< Unique identifier for this node (e.g. "cam0", "sink0").
    std::string type;        ///< Registered type name (e.g. "v4l2src", "tcpsink").
    ParameterSet parameters; ///< Configuration parameters supplied to @ref Node::configure.
};

/**
 * @brief Configuration data describing a directed edge between two nodes.
 *
 * EdgeConfig is a plain data structure that represents one directed connection
 * in the pipeline graph. An edge carries @ref FrameContext data from the output
 * port of the source node to the input port of the target node after each frame.
 *
 * Port names are optional; they may be empty strings for single-port nodes.
 * Multi-port configurations use port names to distinguish independent data streams.
 */
struct EdgeConfig
{
    std::string fromNode; ///< Identifier of the source node.
    std::string fromPort; ///< Output port name on the source node (may be empty).
    std::string toNode;   ///< Identifier of the target node.
    std::string toPort;   ///< Input port name on the target node (may be empty).
};

/**
 * @brief Complete description of a pipeline as a graph of nodes and directed edges.
 *
 * GraphConfig aggregates the full set of @ref NodeConfig and @ref EdgeConfig entries
 * that together define the data-flow topology of a pipeline. It is produced by
 * @ref JsonPipelineParser or @ref CLIPipelineParser and consumed by @ref PipelineBuilder.
 *
 * ### Design
 * GraphConfig is intentionally simple: it is an ordered list of nodes plus a list of
 * edges with no further constraint validation. The parsers ensure structural correctness;
 * the builder validates that all referenced type names are registered in the factory.
 *
 * ### Example
 * @code
 * GraphConfig config;
 * config.addNode({"cam", "v4l2src", params});
 * config.addNode({"sink", "tcpsink", {}});
 * config.addEdge({"cam", "", "sink", ""});
 * @endcode
 *
 * @see NodeConfig
 * @see EdgeConfig
 * @see JsonPipelineParser
 * @see CLIPipelineParser
 * @see PipelineBuilder
 */
class GraphConfig
{
public:
    /**
     * @brief Appends a node configuration to the graph.
     * @param node NodeConfig to add.
     */
    void addNode(const NodeConfig& node);

    /**
     * @brief Appends a directed edge to the graph.
     * @param edge EdgeConfig describing the connection.
     */
    void addEdge(const EdgeConfig& edge);

    /**
     * @brief Finds a mutable node configuration by identifier.
     *
     * Used by parsers to retroactively update a node configuration
     * (e.g. to inject auto-generated identifiers).
     *
     * @param id Node identifier to search for.
     * @return Pointer to the matching @ref NodeConfig, or @c nullptr if not found.
     */
    NodeConfig* findNode(const std::string& id);

    /**
     * @brief Removes a node and all touching edges from the graph.
     * @param id Node identifier to remove.
     * @return @c true if the node existed and was removed.
     */
    bool removeNode(const std::string& id);

    /**
     * @brief Returns the ordered list of all node configurations.
     * @return Read-only reference to the internal node vector.
     */
    std::vector<NodeConfig>& nodes();

    /**
     * @brief Returns the ordered list of all node configurations.
     * @return Read-only reference to the internal node vector.
     */
    const std::vector<NodeConfig>& nodes() const;

    /**
     * @brief Returns the ordered list of all edge configurations.
     * @return Read-only reference to the internal edge vector.
     */
    std::vector<EdgeConfig>& edges();

    /**
     * @brief Returns the ordered list of all edge configurations.
     * @return Read-only reference to the internal edge vector.
     */
    const std::vector<EdgeConfig>& edges() const;

    /**
     * @brief Returns @c true if no nodes have been added yet.
     * @return @c true for an empty graph.
     */
    bool empty() const;

private:
    std::vector<NodeConfig> m_nodes; ///< Ordered list of node configurations.
    std::vector<EdgeConfig> m_edges; ///< Ordered list of edge configurations.
};
