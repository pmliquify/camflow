// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "pipeline/GraphConfig.hpp"
#include "pipeline/IPipeline.hpp"
#include "pipeline/NodeFactory.hpp"
#include "pipeline/PipelineProfiler.hpp"

#include <memory>
#include <atomic>
#include <mutex>
#include <functional>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief Linear, single-threaded pipeline that executes nodes sequentially in registration order.
 *
 * Pipeline is the base implementation of @ref IPipeline. It provides all shared
 * infrastructure — node storage, profiling integration, initialisation, shutdown and
 * node lookup — and can serve as a standalone execution engine or as the base class
 * for @ref PipelineGraph.
 *
 * ### Execution model
 * Each call to @ref run processes frames in an infinite loop (or until @p maxFrames is
 * reached). For every frame a single shared @ref FrameContext is created and passed
 * through all nodes in forward registration order. If any node returns @c false the
 * current frame is aborted and the loop terminates.
 *
 * Graph edges do not change execution order in Pipeline, but they are retained
 * for default scope inference in @ref FrameContext lookups. Nodes still execute in
 * registration order with one shared accumulated @ref FrameContext.
 *
 * ### Profiling
 * Pipeline integrates @ref PipelineProfiler to time every node invocation as well as
 * the complete frame pass (TOTAL). The profiler is available to subclasses through
 * the protected @ref m_profiler member and the @ref runNodeWithProfiling helper.
 *
 * ### Inheritance
 * @ref PipelineGraph extends Pipeline by overriding @ref addEdge and @ref run to
 * implement graph-aware, parallel node dispatch. It reuses the node storage
 * (@ref m_nodes), profiling (@ref m_profiler) and all other shared methods without
 * duplication.
 *
 * @see PipelineGraph
 * @see IPipeline
 * @see PipelineProfiler
 */
class Pipeline : public IPipeline
{
public:
    /**
     * @brief Adds a node and transfers its ownership to the pipeline.
     *
     * The node is appended to the internal node list. In @ref Pipeline the
     * insertion order determines the execution order within each frame.
     *
     * @param node Owning pointer to the node to register.
     */
    void addNode(NodePtr node) override;

    bool removeNode(const std::string& nodeId) override;

    /**
     * @brief Accepts an edge registration for scope inference.
     *
     * Pipeline does not model graph topology for scheduling. All nodes
     * still share a single @ref FrameContext per frame and execute in
     * registration order. Registered edges are used only to infer default
     * input scopes for unqualified context lookups.
     *
     * @param edge Edge configuration (ignored).
     */
    virtual void addEdge(const EdgeConfig& edge) override;

    /**
     * @brief Forwards the profiling flag to the internal @ref PipelineProfiler.
     * @param enabled @c true to activate timing measurements.
     */
    void setProfilingEnabled(bool enabled) override;

    /**
     * @brief Initialises every registered node in forward order.
     *
     * Calls @ref Node::init on each node. Returns @c false and logs the
     * offending node identifier as soon as any init fails.
     *
     * @return @c true if all nodes initialised successfully.
     */
    bool init() override;

    /**
     * @brief Starts every registered node in forward order.
     */
    bool start() override;

    /**
     * @brief Executes the sequential processing loop.
     *
     * For each frame:
     * -# A fresh @ref FrameContext is created.
     * -# Each node is executed in registration order via @ref runNodeWithProfiling.
     * -# If any node returns @c false the frame is considered failed and the loop exits.
     *
     * A full-frame TOTAL timer is started at the beginning of every frame and
     * recorded after all nodes complete (or on failure). When the loop finishes
     * the profiling report is logged if profiling is enabled.
     *
     * @param maxFrames Maximum frames to process; @c 0 = unlimited.
     * @return Number of successfully completed frames.
     */
    virtual int run(int maxFrames) override;

    void setNodeExecutionCallback(const std::function<void(const std::string&, const FrameContext&)>& callback) override;

    void setStopped(bool stopped) override;

    bool isStopped() const override;

    /**
     * @brief Stops all registered nodes in reverse order.
     */
    void stop() override;

    /**
     * @brief Shuts down all nodes in reverse registration order.
     *
     * Calls @ref Node::shutdown on each node starting from the last registered
     * node back to the first, ensuring correct teardown order (sinks before sources).
     */
    void shutdown() override;

    /**
     * @brief Finds a registered node by its identifier.
     *
     * Performs a linear scan over all registered nodes.
     *
     * @param id The identifier assigned via @ref Node::setId.
     * @return Non-owning pointer to the node, or @c nullptr if not found.
     */
    Node* findNode(const std::string& id) override;

protected:
    /**
     * @brief Executes a single node with integrated profiling.
     *
     * Looks up the node with @p nodeId via @ref findNode, then calls
     * @ref PipelineProfiler::start, @ref Node::process and @ref PipelineProfiler::record
     * in sequence. If the node is not found @c false is returned immediately.
     *
     * This method is the central integration point between node execution and the
     * profiler. Subclasses (e.g. @ref PipelineGraph) reuse it so that profiling logic
     * does not need to be duplicated.
     *
     * @param nodeId  Identifier of the node to execute.
     * @param context The @ref FrameContext to pass to the node.
     * @return @c true if the node processed the frame successfully; @c false otherwise.
     */
    bool runNodeWithProfiling(const std::string& nodeId, FrameContext& context);

    /**
     * @brief Returns a default lookup scope inferred from graph connectivity.
     *
     * When a node has no explicit input scopes, Pipeline tries to infer a
     * sensible read order from the registered edges. The result is a best-effort
     * fallback that keeps linear graphs ergonomic without requiring every node to
     * declare its predecessors manually.
     *
     * @param nodeId Node identifier to resolve.
     * @return Inferred scope name, or an empty string if no predecessor exists.
     */
    std::string inferredInputScope(const std::string& nodeId) const;

    /**
     * @brief Returns the node id immediately preceding @p nodeId in registration order.
     *
     * This helper is used as a fallback when graph-based scope inference cannot
     * determine a unique upstream producer.
     *
     * @param nodeId Node identifier to inspect.
     * @return Identifier of the previous node, or an empty string if @p nodeId
     *         is the first registered node or cannot be found.
     */
    std::string previousNodeId(const std::string& nodeId) const;

    /**
     * @brief Returns the ordered list of node identifiers.
     *
     * Returns the node identifiers in registration order. Used by the profiler
     * to control the line order in the report and by @ref PipelineGraph for iteration.
     *
     * @return Vector of node identifier strings.
     */
    std::vector<std::string> nodeOrder() const;

    void waitWhileStopped();

    std::vector<NodePtr> m_nodes;         ///< Ordered list of registered nodes.
    PipelineProfiler m_profiler;          ///< Integrated execution-time profiler.
    std::vector<EdgeConfig> m_scopeEdges; ///< Graph edges used for scope inference.
    std::atomic_bool m_stopped{false};    ///< Runtime stopped state.
    std::atomic_bool m_nodesStopped{false};
    mutable std::mutex m_lifecycleMutex;
    std::function<void(const std::string&, const FrameContext&)> m_nodeExecutionCallback;
};
