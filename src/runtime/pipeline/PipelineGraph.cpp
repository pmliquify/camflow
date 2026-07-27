// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "PipelineGraph.hpp"

#include "core/Logger.hpp"

#include <algorithm>
#include <future>
#include <unordered_map>
#include <vector>

namespace
{
const std::string kTotalProfileId = "__TOTAL__";

std::unordered_map<std::string, size_t> buildScopeConsumerBudget(const std::vector<EdgeConfig>& edges)
{
    std::unordered_map<std::string, size_t> budget;
    for (const auto& edge : edges) {
        if (edge.fromNode.empty() || edge.toNode.empty()) {
            continue;
        }
        budget[edge.fromNode] += 1;
    }
    return budget;
}

std::vector<std::string> consumeScopesForNode(const std::string& nodeId, const std::vector<EdgeConfig>& edges, std::unordered_map<std::string, size_t>& budget)
{
    std::vector<std::string> expired;
    for (const auto& edge : edges) {
        if (edge.toNode != nodeId || edge.fromNode.empty()) {
            continue;
        }

        auto it = budget.find(edge.fromNode);
        if (it == budget.end() || it->second == 0) {
            continue;
        }

        it->second -= 1;
        if (it->second == 0) {
            expired.push_back(edge.fromNode);
        }
    }
    return expired;
}

} // namespace

bool PipelineGraph::removeNode(const std::string& nodeId)
{
    const bool removed = Pipeline::removeNode(nodeId);
    if (!removed) {
        return false;
    }

    m_edges.erase(std::remove_if(m_edges.begin(), m_edges.end(), [&](const EdgeConfig& edge) { return edge.fromNode == nodeId || edge.toNode == nodeId; }), m_edges.end());
    return true;
}

void PipelineGraph::addEdge(const EdgeConfig& edge)
{
    Pipeline::addEdge(edge);
    m_edges.push_back(edge);
}

bool PipelineGraph::start()
{
    if (!m_edgeBridge.setup(m_nodes, m_edges)) {
        return false;
    }
    if (!Pipeline::start()) {
        m_edgeBridge.teardown();
        return false;
    }
    return true;
}

void PipelineGraph::stop()
{
    m_edgeBridge.teardown();
    Pipeline::stop();
}

int PipelineGraph::run(int maxFrames)
{
    // === INITIALIZATION ===
    // Clear the profiler of any previous data to ensure a clean slate for this run.
    // The profiler will track execution time for each node individually and accumulate
    // statistics across all frames processed in this run.
    m_profiler.clear();

    // Guard: if the graph has no nodes, nothing can be executed. Return immediately
    // after logging an empty profiling report (which shows execution timings).
    if (m_nodes.empty()) {
        m_profiler.logReport(nodeOrder());
        return 0;
    }

    const size_t totalLocalNodes = localNodeCount();
    if (totalLocalNodes == 0) {
        m_profiler.logReport(nodeOrder());
        return 0;
    }

    // === FRAME PROCESSING LOOP ===
    // Process frames until either:
    // 1. maxFrames frames have been successfully completed (if maxFrames > 0), or
    // 2. A frame fails (deadlock, node error, etc.) - abort the entire run
    // 3. EOF/input exhaustion (typically controlled by source nodes)
    int frames = 0;
    while (maxFrames <= 0 || frames < maxFrames) {
        if (m_stopped.load()) {
            if (!m_nodesStopped.exchange(true)) {
                std::lock_guard<std::mutex> lock(m_lifecycleMutex);
                stop();
            }
            if (maxFrames > 0) {
                break;
            }
            waitWhileStopped();
            continue;
        }

        if (m_nodesStopped.exchange(false)) {
            std::lock_guard<std::mutex> lock(m_lifecycleMutex);
            if (!start()) {
                LOG_ERROR("Failed to restart nodes after stop state was cleared");
                m_stopped.store(true);
                m_nodesStopped.store(true);
                if (maxFrames > 0) {
                    break;
                }
                waitWhileStopped();
                continue;
            }
        }
        // Start the wall-clock timer for this frame. The __TOTAL__ timer tracks
        // the entire end-to-end execution time including scheduling overhead,
        // async dispatch/join, and context propagation costs.
        m_profiler.start(kTotalProfileId);

        // === PER-FRAME EXECUTION STATE ===
        // Each frame gets a fresh ExecutionState that tracks:
        // - contexts: accumulated FrameContext for each node (input + merged outputs)
        // - completedNodes: list of nodes that have finished this frame
        // - receivedInputs: fan-in count for each node (how many edges have delivered data)
        ExecutionState state;
        m_edgeBridge.beginFrame();
        state.remainingScopeConsumers = buildScopeConsumerBudget(m_edges);
        bool frameOk = true;

        // === GRAPH SCHEDULING LOOP ===
        // Continue until all nodes have executed exactly once. The scheduler is
        // event-driven: ready nodes (all inputs received) are dispatched and executed
        // concurrently. When a node completes its output context is propagated to
        // successor nodes via the edges, incrementing their fan-in count.
        while (state.completedNodes.size() < totalLocalNodes) {
            // Identify which nodes are "ready" to execute:
            // - Source nodes (zero incoming edges) are always ready
            // - Processor/sink nodes become ready once all their incoming edges have
            //   delivered a FrameContext (checked via receivedInputs fan-in count)
            // - Nodes that have already completed are excluded
            auto ready = readyNodes(state);

            // === DEADLOCK DETECTION ===
            // If no node is ready but the frame is incomplete, there is a cycle in the
            // graph or a missing connection. This is a fatal error: abort this frame.
            if (ready.empty()) {
                if (m_edgeBridge.receiveOneRemoteInput(state.contexts, state.receivedInputs)) {
                    continue;
                }
                LOG_ERROR("Pipeline deadlock: no executable node found while frame is incomplete");
                frameOk = false;
                break;
            }

            // === ASYNC TASK DISPATCH ===
            // Prepare a helper struct to capture node execution results (success flag,
            // node ID, and output FrameContext) and return them from the async lambda.
            struct NodeResult
            {
                std::string nodeId;   ///< The node that was executed.
                bool ok = false;      ///< true if Node::process() succeeded.
                FrameContext context; ///< Output context from the node.
            };

            // Launch each ready node as a separate std::async(std::launch::async) task.
            // This ensures true parallelism (separate OS threads, not lazy evaluation).
            // Each task:
            // 1. Captures its input FrameContext (either fresh or merged from predecessors)
            // 2. Calls runNodeWithProfiling(nodeId, context) which calls Node::process(context)
            //    and records timing statistics
            // 3. Returns the result (success flag and output context)
            std::vector<std::future<NodeResult>> futures;
            for (const auto& nodeId : ready) {
                // Retrieve the accumulated input context for this node. If no context
                // exists yet (e.g., a source node with no incoming edges), start with
                // an empty FrameContext. Later-arriving inputs will be merged into
                // the node's context via propagateContext().
                FrameContext inputContext;
                auto existing = state.contexts.find(nodeId);
                if (existing != state.contexts.end()) {
                    inputContext = existing->second;
                }

                // Scope selectors are execution-local and are configured by
                // runNodeWithProfiling for each node. Clear any transient scope
                // state copied from previous contexts.
                inputContext.setWriteScope(std::string());
                inputContext.setReadScopes({});

                // Dispatch the node as an async task with captured node ID and input context.
                // The lambda is marked mutable so it can move the FrameContext without const issues.
                // std::launch::async forces immediate thread creation (not lazy evaluation).
                futures.push_back(std::async(std::launch::async, [this, nodeId, inputContext]() mutable {
                    NodeResult result;
                    result.nodeId = nodeId;
                    result.context = inputContext;
                    // Execute the node with profiling enabled. runNodeWithProfiling:
                    // 1. Calls node->process(result.context) which fills the context with output data
                    // 2. Records execution time and frame count in the profiler
                    // 3. Logs errors if the node fails
                    result.ok = runNodeWithProfiling(nodeId, result.context);
                    return result;
                }));
            }

            // === TASK COMPLETION AND CONTEXT PROPAGATION ===
            // Wait for all dispatched tasks to complete (blocking join). This is safe
            // because the number of pending futures is bounded by the number of nodes
            // in the graph (typically tens to low hundreds, not thousands).
            for (auto& future : futures) {
                // Get the NodeResult from the completed async task. If the task threw
                // an exception, this will re-throw it (which would crash the pipeline).
                // In production, consider wrapping in try-catch and logging gracefully.
                NodeResult result = future.get();

                // If the node execution failed, mark the frame as bad but continue
                // joining remaining futures to avoid leaking thread resources.
                if (!result.ok) {
                    frameOk = false;
                    continue;
                }

                // Mark this node as successfully completed so it won't be scheduled again.
                state.completedNodes.push_back(result.nodeId);

                const auto expiredScopes = consumeScopesForNode(result.nodeId, m_edges, state.remainingScopeConsumers);
                if (!expiredScopes.empty()) {
                    for (const auto& scope : expiredScopes) {
                        result.context.eraseScope(scope);
                    }
                    for (auto& contextEntry : state.contexts) {
                        for (const auto& scope : expiredScopes) {
                            contextEntry.second.eraseScope(scope);
                        }
                    }
                }

                // Propagate the node's output context to all successor nodes via edges.
                // This involves:
                // 1. Merging the output context into each target node's accumulated context
                //    (via FrameContext::mergeFrom, which combines key-value pairs)
                // 2. Incrementing the fan-in counter for each target (receivedInputs)
                // 3. When fan-in == total incoming edges for a node, it becomes ready
                propagateContext(result.nodeId, result.context, state);
            }

            // If any node failed, abort the frame immediately without scheduling more work.
            // This prevents cascading failures and ensures frames are atomic (all-or-nothing).
            if (!frameOk) {
                break;
            }
        }

        // === FRAME COMPLETION ===
        // Record whether the frame succeeded or failed in the profiler's __TOTAL__ timer.
        // The profiler uses this flag to compute statistics separately for successful
        // vs. failed frames (e.g., mean latency for good frames only).
        if (!frameOk) {
            m_profiler.record(kTotalProfileId, false);
            if (m_stopped.load()) {
                if (maxFrames > 0) {
                    break;
                }
                continue;
            }
            // Stop the entire run if a frame failed. Partial success (some frames good,
            // others bad) is not acceptable; the caller expects transactional semantics.
            break;
        }

        m_profiler.record(kTotalProfileId, true);
        ++frames;
    }

    // === SUMMARY AND TEARDOWN ===
    // Log a comprehensive profiling report showing per-node timing statistics
    // (min, max, mean latency; frame throughput) across all successfully completed frames.
    // The report also lists nodes in their execution order for reference.
    m_profiler.logReport(nodeOrder());

    // Return the count of successfully completed frames. Callers can use this to
    // assess throughput or detect partial runs (e.g., if frames < maxFrames,
    // something went wrong or input exhausted).
    return frames;
}

std::vector<std::string> PipelineGraph::sourceNodeIds() const
{
    std::vector<std::string> result;
    for (const auto& node : m_nodes) {
        if (!node || !isNodeLocalRuntime(node->id())) {
            continue;
        }
        if (incomingCount(node->id()) == 0) {
            result.push_back(node->id());
        }
    }
    return result;
}

std::vector<std::string> PipelineGraph::readyNodes(const ExecutionState& state) const
{
    std::vector<std::string> result;
    for (const auto& node : m_nodes) {
        if (!node) {
            continue;
        }
        const std::string& nodeId = node->id();
        if (!isNodeLocalRuntime(nodeId)) {
            continue;
        }
        if (isCompleted(state, nodeId)) {
            continue;
        }
        size_t incoming = incomingCount(nodeId);
        if (incoming == 0) {
            result.push_back(nodeId);
            continue;
        }
        auto it = state.receivedInputs.find(nodeId);
        if (it != state.receivedInputs.end() && it->second >= incoming) {
            result.push_back(nodeId);
        }
    }
    return result;
}

bool PipelineGraph::isCompleted(const ExecutionState& state, const std::string& nodeId) const
{
    return std::find(state.completedNodes.begin(), state.completedNodes.end(), nodeId) != state.completedNodes.end();
}

void PipelineGraph::propagateContext(const std::string& nodeId, const FrameContext& context, ExecutionState& state)
{
    bool hasOutput = false;
    for (const auto& edge : m_edges) {
        if (edge.fromNode != nodeId) {
            continue;
        }

        if (!isNodeLocalRuntime(edge.fromNode)) {
            continue;
        }

        if (m_edgeBridge.sendRemoteContext(edge, context)) {
            hasOutput = true;
            continue;
        }

        hasOutput = true;
        auto& targetContext = state.contexts[edge.toNode];
        targetContext.mergeFrom(context);
        state.receivedInputs[edge.toNode]++;
    }

    if (!hasOutput) {
        state.contexts[nodeId] = context;
    }
}

size_t PipelineGraph::incomingCount(const std::string& nodeId) const
{
    size_t count = 0;
    for (const auto& edge : m_edges) {
        if (edge.toNode == nodeId) {
            ++count;
        }
    }
    return count;
}

bool PipelineGraph::isNodeLocalRuntime(const std::string& nodeId) const
{
    return m_edgeBridge.isNodeLocalRuntime(nodeId);
}

size_t PipelineGraph::localNodeCount() const
{
    return m_edgeBridge.localNodeCount(m_nodes);
}
