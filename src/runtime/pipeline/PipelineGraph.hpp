// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "pipeline/GraphConfig.hpp"
#include "pipeline/Pipeline.hpp"
#include "pipeline/EdgeBridge.hpp"

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Graph-aware, multi-threaded pipeline that executes nodes in dependency order.
 *
 * PipelineGraph extends @ref Pipeline by overriding @ref addEdge and @ref run to
 * implement a full graph execution engine. It inherits all shared infrastructure from
 * @ref Pipeline (node storage, profiling, init, shutdown, findNode) and adds
 * only the graph-specific components:
 *
 * - Directed edge storage (@ref m_edges)
 * - Per-frame @ref ExecutionState tracking which nodes have completed and what
 *   @ref FrameContext data each node has received
 * - Topology-driven scheduling: nodes with no unmet input dependencies become
 *   "ready" and are dispatched concurrently via `std::async(std::launch::async)`
 * - Context propagation: after a node completes its output @ref FrameContext is
 *   merged into the input contexts of all successor nodes
 * - Scope and parameter parity with @ref Pipeline: each node execution uses
 *   @ref Pipeline::runNodeWithProfiling, which applies write/read scopes and
 *   publishes runtime parameters into @ref FrameContext before @ref Node::process
 * - Deadlock detection: if the scheduler finds no ready node while the frame is
 *   incomplete an error is logged and the frame is aborted
 *
 * ### Execution model (per frame)
 * -# A fresh @ref ExecutionState is created.
 * -# Source nodes (zero incoming edges) are immediately ready.
 * -# Ready nodes are collected via @ref readyNodes and launched as async futures.
 * -# When futures complete their output contexts are propagated to successors.
 * -# The cycle repeats until all nodes have completed.
 *
 * ### Profiling
 * Timing is performed through the inherited @ref Pipeline::runNodeWithProfiling
 * helper and the `__TOTAL__` frame-pass timer managed in @ref run. The profiling
 * report is identical in format to the one produced by @ref Pipeline.
 *
 * ### Thread safety
 * The @ref run method launches per-node async tasks. @ref PipelineProfiler is internally
 * thread-safe. The @ref ExecutionState is accessed only from the main scheduling thread
 * after all futures have been joined.
 *
 * @see Pipeline
 * @see IPipeline
 * @see PipelineBuilder
 */
class PipelineGraph : public Pipeline
{
public:
    bool removeNode(const std::string& nodeId) override;

    bool renameNode(const std::string& nodeId, const std::string& newNodeId) override;

    bool start() override;

    void stop() override;

    /**
     * @brief Registers a directed edge between two nodes.
     *
     * Appends the edge to the internal edge list. Edges govern the execution order
     * of nodes and the propagation path of @ref FrameContext data between nodes.
     *
     * Multiple edges may lead to the same target node (fan-in); the target node
     * becomes ready once all its incoming edges have delivered a context for the
     * current frame.
     *
     * @param edge Struct describing the directed connection (fromNode:fromPort → toNode:toPort).
     */
    void addEdge(const EdgeConfig& edge) override;

    /**
     * @brief Runs the graph-aware, multi-threaded processing loop.
     *
     * For each frame:
     * -# A fresh @ref ExecutionState is allocated.
     * -# The scheduler iterates until all nodes have completed:
     *    - Nodes with all input contexts received are collected as "ready".
     *    - Each ready node is dispatched as a separate `std::async` future.
     *    - When futures resolve, output contexts are propagated to successors.
     * -# If no ready nodes exist while the frame is incomplete a deadlock is detected,
     *    an error is logged and the frame is aborted.
     *
     * A full-frame TOTAL profiling timer wraps each frame pass. When the loop
     * finishes the profiling report is logged.
     *
     * @param maxFrames Maximum frames to process; @c 0 = unlimited.
     * @return Number of successfully completed frames.
     */
    int run(int maxFrames) override;

private:
    /**
     * @brief Tracks the execution state for a single frame pass.
     *
     * ExecutionState is created fresh for every frame and accumulates:
     * - per-node output @ref FrameContext values (forwarded to successor inputs)
     * - the count of received input contexts per node (fan-in tracking)
     * - the list of already-completed node identifiers
     */
    struct ExecutionState
    {
        std::map<std::string, FrameContext> contexts;                    ///< Accumulated input contexts per node.
        std::map<std::string, size_t> receivedInputs;                    ///< Number of delivered inputs per node.
        std::vector<std::string> completedNodes;                         ///< Identifiers of nodes that have finished.
        std::unordered_map<std::string, size_t> remainingScopeConsumers; ///< Remaining nodes that still need each producer scope.
    };

    /**
     * @brief Returns the identifiers of all nodes that have no incoming edges.
     *
     * Source nodes always become ready at the start of a frame and are
     * dispatched first by the scheduler.
     *
     * @return Vector of source node identifiers.
     */
    std::vector<std::string> sourceNodeIds() const;

    /**
     * @brief Returns the identifiers of all nodes that are ready to run.
     *
     * A node is ready if it has not yet completed and all its incoming edges
     * have delivered a @ref FrameContext for the current frame.
     * Source nodes (zero incoming edges) are always ready.
     *
     * @param state Current frame execution state.
     * @return Vector of ready node identifiers.
     */
    std::vector<std::string> readyNodes(const ExecutionState& state) const;

    /**
     * @brief Checks whether a node has already completed in the current frame.
     *
     * @param state  Current frame execution state.
     * @param nodeId Node identifier to check.
     * @return @c true if the node has already been executed this frame.
     */
    bool isCompleted(const ExecutionState& state, const std::string& nodeId) const;

    /**
     * @brief Propagates the output context of a node to its successors.
     *
     * After a node finishes, its output @ref FrameContext is merged into the
     * input context of every node connected via an outgoing edge. The received-
     * inputs counter for each successor is incremented. If the node has no
     * outgoing edges its context is stored under its own id for potential lookup.
     *
     * @param nodeId  Identifier of the node that just completed.
     * @param context The output @ref FrameContext produced by the node.
     * @param state   Current frame execution state (modified in place).
     */
    void propagateContext(const std::string& nodeId, const FrameContext& context, ExecutionState& state);

    /**
     * @brief Returns the number of edges that point to the given node.
     *
     * Used by @ref readyNodes to determine when all inputs for a node are available.
     *
     * @param nodeId Node identifier to count incoming edges for.
     * @return Number of registered incoming edges.
     */
    size_t incomingCount(const std::string& nodeId) const;

    bool isNodeLocalRuntime(const std::string& nodeId) const;
    size_t localNodeCount() const;

    std::vector<EdgeConfig> m_edges; ///< Registered directed edges between nodes.
    EdgeBridge m_edgeBridge;
};
