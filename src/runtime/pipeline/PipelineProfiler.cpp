// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "PipelineProfiler.hpp"

#include "core/Logger.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>

namespace
{
const std::string kTotalProfileId = "__TOTAL__";
}

void PipelineProfiler::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

bool PipelineProfiler::enabled() const
{
    return m_enabled;
}

void PipelineProfiler::clear()
{
    std::scoped_lock lock(m_mutex);
    m_starts.clear();
    m_stats.clear();
}

void PipelineProfiler::start(const std::string& nodeId)
{
    if (!m_enabled) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    std::scoped_lock lock(m_mutex);
    m_starts[nodeId] = now;
}

void PipelineProfiler::record(const std::string& nodeId)
{
    record(nodeId, true);
}

void PipelineProfiler::record(const std::string& nodeId, bool ok)
{
    if (!m_enabled) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    std::scoped_lock lock(m_mutex);
    auto it = m_starts.find(nodeId);
    if (it == m_starts.end()) {
        return;
    }

    const uint64_t durationNs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - it->second).count());
    m_starts.erase(it);
    recordDuration(nodeId, durationNs, ok);
}

void PipelineProfiler::recordDuration(const std::string& nodeId, uint64_t durationNs, bool ok)
{
    auto& stat = m_stats[nodeId];
    ++stat.calls;
    if (!ok) {
        ++stat.failures;
    }
    stat.totalNs += durationNs;
    if (stat.calls == 1) {
        stat.minNs = durationNs;
        stat.maxNs = durationNs;
    } else {
        stat.minNs = std::min(stat.minNs, durationNs);
        stat.maxNs = std::max(stat.maxNs, durationNs);
    }
}

std::string PipelineProfiler::formatMs(uint64_t durationNs) const
{
    std::ostringstream text;
    text << std::fixed << std::setprecision(3) << (static_cast<double>(durationNs) / 1e6);
    return text.str();
}

void PipelineProfiler::logReport(const std::vector<std::string>& nodeOrder) const
{
    if (!m_enabled) {
        return;
    }

    std::scoped_lock lock(m_mutex);

    LOG_INFO("Node profiling report:");
    if (m_stats.empty()) {
        LOG_INFO("  (no node executions)");
        return;
    }

    bool haveNodeStats = false;
    uint64_t fallbackTotalCalls = 0;
    uint64_t fallbackTotalFailures = 0;
    uint64_t fallbackTotalNs = 0;
    uint64_t fallbackMinNs = std::numeric_limits<uint64_t>::max();
    uint64_t fallbackMaxNs = 0;

    for (const auto& nodeId : nodeOrder) {
        if (nodeId == kTotalProfileId) {
            continue;
        }

        auto it = m_stats.find(nodeId);
        if (it == m_stats.end()) {
            LOG_INFO("  " + nodeId + ": calls=0");
            continue;
        }

        const Stat& stat = it->second;
        const uint64_t avgNs = stat.calls > 0 ? (stat.totalNs / stat.calls) : 0;

        LOG_INFO("  " + nodeId + ": calls=" + std::to_string(stat.calls) + ", fail=" + std::to_string(stat.failures) + ", avg=" + formatMs(avgNs) + " ms, min=" + formatMs(stat.minNs) +
                 " ms, max=" + formatMs(stat.maxNs) + " ms, total=" + formatMs(stat.totalNs) + " ms");

        fallbackTotalFailures += stat.failures;
        fallbackTotalNs += stat.totalNs;
        fallbackMinNs = std::min(fallbackMinNs, stat.minNs);
        fallbackMaxNs = std::max(fallbackMaxNs, stat.maxNs);
        if (!haveNodeStats) {
            fallbackTotalCalls = stat.calls;
            haveNodeStats = true;
        } else {
            // A complete pass is only valid for calls that all nodes share.
            fallbackTotalCalls = std::min(fallbackTotalCalls, stat.calls);
        }
    }

    auto totalIt = m_stats.find(kTotalProfileId);
    if (totalIt != m_stats.end()) {
        const Stat& total = totalIt->second;
        const uint64_t totalAvgNs = total.calls > 0 ? (total.totalNs / total.calls) : 0;
        LOG_INFO("  TOTAL: calls=" + std::to_string(total.calls) + ", fail=" + std::to_string(total.failures) + ", avg=" + formatMs(totalAvgNs) + " ms, min=" + formatMs(total.minNs) +
                 " ms, max=" + formatMs(total.maxNs) + " ms, total=" + formatMs(total.totalNs) + " ms");
        return;
    }

    if (!haveNodeStats) {
        LOG_INFO("  TOTAL: calls=0");
        return;
    }

    const uint64_t totalAvgNs = fallbackTotalCalls > 0 ? (fallbackTotalNs / fallbackTotalCalls) : 0;
    LOG_INFO("  TOTAL: calls=" + std::to_string(fallbackTotalCalls) + ", fail=" + std::to_string(fallbackTotalFailures) + ", avg=" + formatMs(totalAvgNs) + " ms, min=" + formatMs(fallbackMinNs) +
             " ms, max=" + formatMs(fallbackMaxNs) + " ms, total=" + formatMs(fallbackTotalNs) + " ms");
}
