// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

/**
 * @brief Thread-safe, per-node execution time profiler for pipeline implementations.
 *
 * PipelineProfiler measures wall-clock time for individual node invocations inside a
 * pipeline execution loop and aggregates the results per node. It also tracks an
 * end-to-end frame-pass measurement under the reserved key `__TOTAL__` so that the
 * aggregate TOTAL entry in the report always reflects the real wall-clock duration of
 * one complete frame pass (which equals or exceeds the sum of all per-node times).
 *
 * ### Usage pattern
 * @code
 * // Enable once before the first run:
 * profiler.setEnabled(true);
 *
 * // In the processing loop – one frame pass:
 * profiler.start("__TOTAL__");
 * for (const auto& nodeId : nodes) {
 *     profiler.start(nodeId);
 *     bool ok = node->process(context);
 *     profiler.record(nodeId, ok);
 * }
 * profiler.record("__TOTAL__", frameOk);
 *
 * // At shutdown:
 * profiler.logReport(orderedNodeIds);
 * @endcode
 *
 * ### Thread safety
 * All public methods are protected by an internal `std::mutex`. It is safe to call
 * @ref start and @ref record from different threads concurrently (as done by @ref PipelineGraph
 * which runs multiple nodes via `std::async`).
 *
 * ### Report format
 * Each line in the report has the form:
 * @code
 *   <nodeId>: calls=N, fail=M, avg=X.XXX ms, min=Y.YYY ms, max=Z.ZZZ ms, total=T.TTT ms
 * @endcode
 * followed by one TOTAL line with end-to-end frame-pass statistics.
 *
 * @see Pipeline::run
 * @see PipelineGraph::run
 */
class PipelineProfiler
{
public:
    /**
     * @brief Enables or disables profiling.
     *
     * When disabled all @ref start and @ref record calls are no-ops, and
     * @ref logReport produces no output. Profiling is disabled by default.
     *
     * @param enabled @c true to enable profiling, @c false to disable.
     */
    void setEnabled(bool enabled);

    /**
     * @brief Returns whether profiling is currently enabled.
     * @return @c true if profiling is active.
     */
    bool enabled() const;

    /**
     * @brief Resets all accumulated statistics and pending start timestamps.
     *
     * Should be called at the beginning of each @ref IPipeline::run invocation
     * to start accumulation from a clean state.
     */
    void clear();

    /**
     * @brief Records the start of a timed measurement for the given node.
     *
     * Captures `std::chrono::steady_clock::now()` and stores it under @p nodeId.
     * A subsequent call to @ref record(const std::string&, bool) with the same
     * @p nodeId computes the elapsed duration and updates the statistics.
     *
     * If profiling is disabled this method is a no-op.
     *
     * @param nodeId Unique node identifier (or `"__TOTAL__"` for full-frame timing).
     */
    void start(const std::string& nodeId);

    /**
     * @brief Records the end of a measurement assuming the node succeeded.
     *
     * Convenience overload of @ref record(const std::string&, bool) with @p ok = @c true.
     *
     * @param nodeId The same identifier passed to the matching @ref start call.
     */
    void record(const std::string& nodeId);

    /**
     * @brief Records the end of a timed measurement for the given node.
     *
     * Computes the elapsed nanoseconds since the corresponding @ref start call,
     * removes the pending start entry and updates the per-node statistics:
     * - increments the call counter
     * - increments the failure counter if @p ok is @c false
     * - adds the duration to the total accumulated time
     * - updates the running minimum and maximum durations
     *
     * If no matching @ref start entry exists (e.g. profiling was disabled at
     * start time) the call is silently ignored.
     *
     * @param nodeId The same identifier passed to the matching @ref start call.
     * @param ok     @c true if the node's @ref Node::process call succeeded.
     */
    void record(const std::string& nodeId, bool ok);

    /**
     * @brief Logs the accumulated profiling statistics to the application logger.
     *
     * Iterates over @p nodeOrder and prints one log line per node with the
     * following statistics (times in milliseconds, 3 decimal places):
     * - `calls`  – number of invocations
     * - `fail`   – number of failed invocations
     * - `avg`    – average duration per call
     * - `min`    – minimum observed duration
     * - `max`    – maximum observed duration
     * - `total`  – cumulative duration
     *
     * A final TOTAL line is printed using the dedicated `__TOTAL__` statistics
     * entry if it exists (represents the real end-to-end frame-pass time), or
     * falls back to a derived estimate from per-node data.
     *
     * Does nothing if profiling is disabled.
     *
     * @param nodeOrder Ordered list of node identifiers; controls line order in the report.
     */
    void logReport(const std::vector<std::string>& nodeOrder) const;

private:
    /// Per-node accumulated timing statistics.
    struct Stat
    {
        uint64_t calls = 0;    ///< Total number of process() invocations.
        uint64_t failures = 0; ///< Number of invocations that returned false.
        uint64_t totalNs = 0;  ///< Sum of all measured durations in nanoseconds.
        uint64_t minNs = 0;    ///< Minimum observed duration in nanoseconds.
        uint64_t maxNs = 0;    ///< Maximum observed duration in nanoseconds.
    };

    /** @brief Converts nanoseconds to a millisecond string with 3 decimal places. */
    std::string formatMs(uint64_t durationNs) const;

    /** @brief Internal helper that updates the Stat entry without locking (caller holds lock). */
    void recordDuration(const std::string& nodeId, uint64_t durationNs, bool ok);

    bool m_enabled = false;                                                ///< Whether profiling is active.
    mutable std::mutex m_mutex;                                            ///< Protects all mutable state.
    std::map<std::string, std::chrono::steady_clock::time_point> m_starts; ///< Pending start timestamps.
    std::map<std::string, Stat> m_stats;                                   ///< Accumulated statistics per node.
};
