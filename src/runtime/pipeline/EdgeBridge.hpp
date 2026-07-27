// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "network/ImageSocket.hpp"
#include "pipeline/GraphConfig.hpp"
#include "pipeline/IPipeline.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * @brief Bridges edges between local and remote runtimes.
 *
 * EdgeBridge inspects graph edges and node runtime placement to decide whether an edge
 * stays local or must be transported over TCP. For remote edges it creates transport
 * channels backed by @ref ImageSocketClient and @ref ImageSocketServer.
 *
 * The bridge transfers scoped @ref FrameContext payloads and tracks which remote inputs
 * were already received in the current frame iteration.
 */
class EdgeBridge
{
public:
    /**
     * @brief Builds incoming and outgoing transport channels for remote edges.
     *
     * @param nodes Live pipeline nodes.
     * @param edges Graph edges to inspect.
     * @return @c true if setup succeeded.
     */
    bool setup(const std::vector<NodePtr>& nodes, const std::vector<EdgeConfig>& edges);

    /** @brief Closes all transport channels and clears placement state. */
    void teardown();

    /** @brief Resets per-frame receive bookkeeping. */
    void beginFrame();

    /** @brief Returns @c true when the node belongs to a local runtime alias. */
    bool isNodeLocalRuntime(const std::string& nodeId) const;

    /** @brief Counts nodes that run on local runtime aliases. */
    size_t localNodeCount(const std::vector<NodePtr>& nodes) const;

    /**
     * @brief Receives one remote input payload and merges it into per-node contexts.
     *
     * @param contexts Mutable frame contexts indexed by target node id.
     * @param receivedInputs Mutable counter of already received remote inputs per node.
     * @return @c true if one payload was received and merged.
     */
    bool receiveOneRemoteInput(std::map<std::string, FrameContext>& contexts, std::map<std::string, size_t>& receivedInputs);

    /**
     * @brief Sends the remote subset of one edge context to the target runtime.
     *
     * @param edge Source graph edge.
     * @param context Frame context produced by source node.
     * @return @c true on success.
     */
    bool sendRemoteContext(const EdgeConfig& edge, const FrameContext& context);

private:
    /** @brief Runtime placement metadata for one node id. */
    struct NodePlacement
    {
        std::string runtimeHost;
        bool local = true;
    };

    /** @brief Incoming remote edge channel with bound server endpoint. */
    struct RemoteIncomingChannel
    {
        std::string id;
        EdgeConfig edge;
        std::string sourceHost;
        uint16_t port = 0;
        ImageSocketServer server;
    };

    /** @brief Outgoing remote edge channel with connected client endpoint. */
    struct RemoteOutgoingChannel
    {
        std::string id;
        EdgeConfig edge;
        std::string targetHost;
        uint16_t port = 0;
        ImageSocketClient client;
    };

    static std::string normalizeRuntimeHost(const std::string& value);
    static std::string edgeTransportId(const EdgeConfig& edge);
    static uint16_t edgeTransportPort(const std::string& edgeId);
    static std::unordered_set<std::string> collectLocalRuntimeAliases();

    std::vector<std::string> scopedKeys(const FrameContext& context, const std::string& scope) const;

    NodePlacement placementForNode(const std::string& nodeId) const;

    std::unordered_set<std::string> m_localRuntimeAliases;
    std::unordered_map<std::string, NodePlacement> m_nodePlacements;
    std::vector<std::unique_ptr<RemoteIncomingChannel>> m_remoteIncoming;
    std::vector<std::unique_ptr<RemoteOutgoingChannel>> m_remoteOutgoing;
    std::unordered_map<std::string, RemoteOutgoingChannel*> m_remoteOutgoingById;
    std::unordered_set<std::string> m_receivedRemoteChannels;
};
