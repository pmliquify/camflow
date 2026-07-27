// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "parameters/Parameter.hpp"
#include "pipeline/GraphConfig.hpp"
#include "pipeline/IPipeline.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/**
 * @brief Thread-safe controller that owns the running pipeline and exposes runtime control operations.
 *
 * RuntimeController acts as the bridge between the running @ref IPipeline instance and
 * external control paths such as the @ref WebServer. It owns the pipeline and the
 * current @ref GraphConfig and serialises all modifications through an internal mutex.
 *
 * ### Responsibilities
 * - Stores the running pipeline and its associated graph configuration.
 * - Forwards parameter update requests to individual nodes.
 * - Provides JSON serialisation of the current graph and node parameters for the REST API.
 * - Supports hot-swapping the graph at runtime via @ref replaceGraph.
 *
 * ### Thread safety
 * All public methods acquire `m_mutex` before accessing the pipeline or graph. It is
 * safe to call any method from the REST server thread while the pipeline is running
 * on the main thread.
 *
 * @see IPipeline
 * @see WebServer
 */
class RuntimeController
{
public:
    using FrameContextObserver = std::function<void(const std::string&, const FrameContext&)>;
    using PipelineRebuildHandler = std::function<std::unique_ptr<IPipeline>(const GraphConfig&, bool stopped)>;
    using IncrementalNodeFactory = std::function<NodePtr(const NodeConfig&)>;

    /**
     * @brief Takes ownership of @p pipeline and stores the associated @p graph.
     *
     * Called once after the pipeline is built and initialised. After this call
     * the controller is responsible for the pipeline's lifetime.
     *
     * @param pipeline Owning pointer to the initialised pipeline.
     * @param graph    The graph configuration used to build the pipeline.
     */
    void setPipeline(std::unique_ptr<IPipeline> pipeline, const GraphConfig& graph);

    /** @brief Installs the callback used to rebuild the pipeline when the graph is replaced. */
    void setPipelineRebuildHandler(const PipelineRebuildHandler& handler);

    /** @brief Installs the callback used to instantiate a single node for incremental graph edits. */
    void setIncrementalNodeFactory(const IncrementalNodeFactory& factory);

    /**
     * @brief Updates a single named parameter on the specified node.
     *
     * Finds the node with @p nodeId and calls @ref Node::setParameter with the
     * given typed value. Thread-safe.
     *
     * @param nodeId        Identifier of the target node.
     * @param parameterName Name of the parameter to update.
     * @param value         New parameter value.
     * @return @c true if the node was found and accepted the new value.
     */
    bool setParameter(const std::string& nodeId, const std::string& parameterName, const ParameterValue& value);

    /**
     * @brief Updates a parameter from a raw string value.
     *
     * Looks up the parameter type from the node's schema, parses @p value via
     * @ref parameterValueFromString and calls @ref setParameter.
     *
     * @param nodeId        Identifier of the target node.
     * @param parameterName Name of the parameter to update.
     * @param value         String representation of the new value.
     * @return @c true if the parameter was found, parsed and accepted.
     */
    bool setParameterFromString(const std::string& nodeId, const std::string& parameterName, const std::string& value);

    /**
     * @brief Replaces the current graph configuration (hot-swap).
     *
     * Updates the stored @ref GraphConfig. Does not rebuild or restart the
     * pipeline; this method is reserved for future live-reconfiguration support.
     *
     * @param config The new graph configuration.
     * @return @c true unconditionally (reserved for future validation).
     */
    bool replaceGraph(const GraphConfig& config);

    /** @brief Adds one node to the existing stopped pipeline without rebuilding the whole graph. */
    bool addNode(NodeConfig& config);

    /** @brief Adds one edge to the existing stopped pipeline without rebuilding the whole graph. */
    bool addEdge(const EdgeConfig& edge);

    /** @brief Removes one node from the existing stopped pipeline without rebuilding the whole graph. */
    bool removeNode(const std::string& nodeId);

    /** @brief Removes one edge from the graph and rebuilds the stopped pipeline when needed. */
    bool removeEdge(const EdgeConfig& edge);

    /**
     * @brief Returns a JSON string describing the current graph.
     *
     * Serialises all nodes and edges from the stored @ref GraphConfig into a
     * JSON document matching the format expected by @ref JsonPipelineParser.
     *
     * @return JSON string, or an empty string if no pipeline is set.
     */
    std::string graphJson() const;

    /**
     * @brief Returns a JSON string with the current parameter values for a node.
     *
     * Serialises the active @ref ParameterSet of the node identified by @p nodeId.
     *
     * @param nodeId Identifier of the node to query.
     * @return JSON string of name-value pairs, or an empty string if the node is not found.
     */
    std::string nodeParametersJson(const std::string& nodeId) const;

    /**
     * @brief Returns the @ref NodeConfig for the given node identifier.
     *
     * @param nodeId Identifier to look up.
     * @param config Output parameter; filled with the node configuration on success.
     * @return @c true if the node was found in the stored graph.
     */
    bool nodeConfig(const std::string& nodeId, NodeConfig& config) const;

    /**
     * @brief Returns the live schema and current parameters of a running node.
     *
     * Uses the live node instance when a pipeline is active so runtime-discovered
     * parameters such as V4L2 controls are included.
     *
     * @param nodeId      Identifier to look up.
     * @param schema      Output parameter for the current schema.
     * @param parameters  Output parameter for current parameter values.
     * @return @c true if the node exists in the running pipeline.
     */
    bool nodeState(const std::string& nodeId, NodeSchema& schema, ParameterSet& parameters) const;

    /** @brief Stops or starts the running pipeline. */
    bool setStopped(bool stopped);

    /** @brief Returns whether the running pipeline is currently stopped. */
    bool isStopped() const;

    /**
     * @brief Registers a frame-context observer.
     *
     * The observer is notified after each node execution.
     */
    void addFrameContextObserver(const FrameContextObserver& observer);

    /** @brief Returns @c true when the pipeline is currently stopped. */
    bool pipelineStopped() const;

    /** @brief Runs a bounded number of frames on the current pipeline. */
    int runFrames(int maxFrames);

private:
    void notifyFrameContextObservers(const std::string& nodeId, const FrameContext& context);

    std::unique_ptr<IPipeline> m_pipeline; ///< Owned running pipeline instance.
    GraphConfig m_graph;                   ///< Current graph configuration.
    mutable std::mutex m_mutex;            ///< Guards all access to pipeline and graph.
    std::vector<FrameContextObserver> m_frameContextObservers;
    mutable std::mutex m_observerMutex;
    PipelineRebuildHandler m_pipelineRebuildHandler;
    IncrementalNodeFactory m_incrementalNodeFactory;
};
