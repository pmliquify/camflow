// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "pipeline/GraphConfig.hpp"
#include "pipeline/NodeFactory.hpp"

#include <functional>
#include <string>

/**
 * @brief Abstract interface defining the lifecycle and execution contract for all pipeline implementations.
 *
 * IPipeline is the central abstraction layer between application code and the concrete pipeline
 * execution strategy. Every pipeline variant — whether sequential or graph-aware — implements
 * this interface so that callers can be agnostic to the chosen execution model.
 *
 * Two concrete implementations are available:
 * - @ref Pipeline – sequential, single-threaded linear execution that processes nodes in
 *   registration order and shares a single @ref FrameContext across all nodes per frame.
 *   Graph edges are accepted but ignored.
 * - @ref PipelineGraph – graph-aware, multi-threaded execution built on top of @ref Pipeline.
 *   Nodes are dispatched in dependency order according to registered edges; independent nodes
 *   run concurrently via `std::async`.
 *
 * The pipeline mode is selected at construction time through @ref PipelineBuilder::build.
 * The CLI flag `--simple-pipeline` / `-s` forces the use of @ref Pipeline.
 *
 * ### Typical lifecycle
 * @code
 * auto pipeline = builder.build(config, useSimplePipeline);
 * pipeline->setProfilingEnabled(enableProfiling);
 * pipeline->init();
 * pipeline->start();
 * int frames = pipeline->run(maxFrames);
 * pipeline->stop();
 * pipeline->shutdown();
 * @endcode
 *
 * @see Pipeline
 * @see PipelineGraph
 * @see PipelineBuilder
 */
class IPipeline
{
public:
    virtual ~IPipeline() = default;

    /**
     * @brief Adds a node to the pipeline.
     *
     * Transfers exclusive ownership of the node to the pipeline. The pipeline stores
     * nodes in insertion order. For @ref Pipeline this order determines the
     * per-frame execution sequence. For @ref PipelineGraph the graph topology drives
     * execution order instead.
     *
     * @param node Owning smart-pointer to the node instance to add.
     */
    virtual void addNode(NodePtr node) = 0;

    /**
     * @brief Removes a node from the pipeline by identifier.
     *
     * Implementations should preserve all remaining node instances and only tear down
     * the removed node plus any touching connectivity state.
     *
     * @param nodeId Identifier of the node to remove.
     * @return @c true if a node was removed.
     */
    virtual bool removeNode(const std::string& nodeId) = 0;

    /**
     * @brief Renames a node and updates internal edge references without rebuilding the pipeline.
     * @param nodeId Current node identifier.
     * @param newNodeId Requested replacement identifier.
     * @return @c true when the node was found and the new identifier was unused.
     */
    virtual bool renameNode(const std::string& nodeId, const std::string& newNodeId) = 0;

    /**
     * @brief Registers a directed data edge between two nodes.
     *
     * An edge describes that the @ref FrameContext produced by @p edge.fromNode
     * (port @p edge.fromPort) is forwarded to @p edge.toNode (port @p edge.toPort)
     * after each successful node execution. Implementations that do not model graph
     * topology — such as @ref Pipeline — silently ignore edge registrations.
     *
     * @param edge Struct describing the directed connection (from-node/port → to-node/port).
     */
    virtual void addEdge(const EdgeConfig& edge) = 0;

    /**
     * @brief Enables or disables per-node execution time profiling.
     *
     * When enabled, every node invocation is timed using a monotonic clock
     * (@ref PipelineProfiler). At the end of @ref run the pipeline logs a full
     * report that includes per-node statistics (calls, failures, avg/min/max/total)
     * and an aggregate TOTAL row representing the end-to-end duration per frame pass.
     * Times are printed with 3 decimal places in milliseconds.
     *
     * @param enabled @c true to activate profiling, @c false to deactivate.
     */
    virtual void setProfilingEnabled(bool enabled) = 0;

    /**
     * @brief Initialises all registered nodes before the first frame is processed.
     *
     * Iterates over all registered nodes and calls @ref Node::init on each one
     * in forward registration order. If any node fails to initialise the method
     * returns @c false immediately and logs the failing node identifier.
     *
     * Must be called exactly once after @ref addNode / @ref addEdge and before
     * the first call to @ref run.
     *
     * @return @c true if every node initialised successfully; @c false otherwise.
     */
    virtual bool init() = 0;

    /**
     * @brief Starts all registered nodes after initialisation.
     *
     * Iterates over all registered nodes and calls @ref Node::start on each one
     * in forward registration order. If any node fails to start the method returns
     * @c false immediately and logs the failing node identifier.
     */
    virtual bool start() = 0;

    /**
     * @brief Runs the processing loop for the given number of frames.
     *
     * Drives the pipeline until either @p maxFrames complete frames have been
     * processed or a node signals end-of-stream by returning @c false from
     * @ref Node::process. A value of @c 0 for @p maxFrames means unlimited
     * processing until end-of-stream.
     *
     * If profiling was enabled via @ref setProfilingEnabled the full profiling
     * report is emitted to the logger before the method returns.
     *
     * @param maxFrames Maximum number of frames to process; @c 0 = unlimited.
     * @return Number of frames that completed successfully.
     */
    virtual int run(int maxFrames) = 0;

    /**
     * @brief Installs a callback invoked after each node execution.
     *
     * The callback receives the executed node id and the current frame context.
     * It is used for live diagnostics and streaming telemetry.
     */
    virtual void setNodeExecutionCallback(const std::function<void(const std::string&, const FrameContext&)>& callback) = 0;

    /**
     * @brief Stops or starts frame processing.
     *
     * When stopped, implementations should stop all nodes (e.g. capture sources stop
     * streaming) and block new frame processing until started again. A frame already in
     * progress may still complete before the stop takes effect.
     *
     * @param stopped @c true to stop processing, @c false to start again.
     */
    virtual void setStopped(bool stopped) = 0;

    /**
     * @brief Returns whether the pipeline is currently stopped.
     * @return @c true when stopped.
     */
    virtual bool isStopped() const = 0;

    /**
     * @brief Stops all nodes in reverse registration order.
     */
    virtual void stop() = 0;

    /**
     * @brief Shuts down all nodes in reverse registration order.
     *
     * Calls @ref Node::shutdown on every node starting from the last registered
     * node back to the first. This ensures that sinks are torn down before
     * processors and sources, matching the natural resource-release order.
     */
    virtual void shutdown() = 0;

    /**
     * @brief Finds a registered node by its unique string identifier.
     *
     * Performs a linear scan over the registered nodes and returns a raw
     * (non-owning) pointer to the first node whose id matches @p id.
     *
     * @param id The node identifier assigned via @ref Node::setId.
     * @return Non-owning pointer to the matching node, or @c nullptr if not found.
     */
    virtual Node* findNode(const std::string& id) = 0;
};
