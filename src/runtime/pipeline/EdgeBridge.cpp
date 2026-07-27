// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "EdgeBridge.hpp"

#include "core/Logger.hpp"

#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

std::string EdgeBridge::normalizeRuntimeHost(const std::string& value)
{
    std::string host = value;
    host.erase(0, host.find_first_not_of(" \t\r\n"));
    host.erase(host.find_last_not_of(" \t\r\n") + 1);
    if (host.empty()) {
        return std::string();
    }

    const size_t scheme = host.find("://");
    if (scheme != std::string::npos) {
        host = host.substr(scheme + 3);
    }
    const size_t slash = host.find('/');
    if (slash != std::string::npos) {
        host = host.substr(0, slash);
    }
    const size_t colon = host.find(':');
    if (colon != std::string::npos) {
        host = host.substr(0, colon);
    }
    return host;
}

std::string EdgeBridge::edgeTransportId(const EdgeConfig& edge)
{
    return edge.fromNode + ":" + edge.fromPort + "->" + edge.toNode + ":" + edge.toPort;
}

uint16_t EdgeBridge::edgeTransportPort(const std::string& edgeId)
{
    uint32_t hash = 2166136261u;
    for (const char c : edgeId) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }
    return static_cast<uint16_t>(20000u + (hash % 30000u));
}

std::unordered_set<std::string> EdgeBridge::collectLocalRuntimeAliases()
{
    std::unordered_set<std::string> aliases;
    aliases.insert("");
    aliases.insert("localhost");
    aliases.insert("127.0.0.1");
    aliases.insert("0.0.0.0");

    char hostName[256] = {0};
    if (::gethostname(hostName, sizeof(hostName) - 1) == 0) {
        aliases.insert(std::string(hostName));

        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* info = nullptr;
        if (::getaddrinfo(hostName, nullptr, &hints, &info) == 0) {
            for (addrinfo* node = info; node != nullptr; node = node->ai_next) {
                char address[NI_MAXHOST] = {0};
                if (::getnameinfo(node->ai_addr, node->ai_addrlen, address, sizeof(address), nullptr, 0, NI_NUMERICHOST) == 0) {
                    aliases.insert(std::string(address));
                }
            }
            ::freeaddrinfo(info);
        }
    }

    ifaddrs* addresses = nullptr;
    if (::getifaddrs(&addresses) == 0) {
        for (ifaddrs* current = addresses; current != nullptr; current = current->ifa_next) {
            if (current->ifa_addr == nullptr) {
                continue;
            }
            const int family = current->ifa_addr->sa_family;
            if (family != AF_INET && family != AF_INET6) {
                continue;
            }
            char address[NI_MAXHOST] = {0};
            if (::getnameinfo(current->ifa_addr, family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6), address, sizeof(address), nullptr, 0, NI_NUMERICHOST) == 0) {
                aliases.insert(std::string(address));
            }
        }
        ::freeifaddrs(addresses);
    }

    return aliases;
}

EdgeBridge::NodePlacement EdgeBridge::placementForNode(const std::string& nodeId) const
{
    const auto it = m_nodePlacements.find(nodeId);
    if (it != m_nodePlacements.end()) {
        return it->second;
    }
    return NodePlacement{};
}

std::vector<std::string> EdgeBridge::scopedKeys(const FrameContext& context, const std::string& scope) const
{
    std::vector<std::string> keys;
    const std::string prefix = scope + ".";
    for (const auto& key : context.keys()) {
        if (key == scope || key.rfind(prefix, 0) == 0) {
            keys.push_back(key);
        }
    }
    return keys;
}

bool EdgeBridge::setup(const std::vector<NodePtr>& nodes, const std::vector<EdgeConfig>& edges)
{
    teardown();

    m_localRuntimeAliases = collectLocalRuntimeAliases();
    m_nodePlacements.clear();

    for (const auto& node : nodes) {
        if (!node) {
            continue;
        }
        const auto parameters = node->currentParameters();
        std::string runtimeTarget = parameters.stringValue("runtimeTargetIp", std::string());
        if (runtimeTarget.empty()) {
            runtimeTarget = parameters.stringValue("runtimeId", std::string());
        }
        if (runtimeTarget.empty()) {
            runtimeTarget = parameters.stringValue("runtime", std::string());
        }
        runtimeTarget = normalizeRuntimeHost(runtimeTarget);

        NodePlacement placement;
        placement.runtimeHost = runtimeTarget;
        placement.local = runtimeTarget.empty() || m_localRuntimeAliases.find(runtimeTarget) != m_localRuntimeAliases.end();
        m_nodePlacements[node->id()] = placement;
    }

    size_t incomingCount = 0;
    size_t outgoingCount = 0;
    for (const auto& edge : edges) {
        const bool fromLocal = isNodeLocalRuntime(edge.fromNode);
        const bool toLocal = isNodeLocalRuntime(edge.toNode);
        if (fromLocal && !toLocal) {
            ++outgoingCount;
        } else if (!fromLocal && toLocal) {
            ++incomingCount;
        }
    }

    m_remoteIncoming.reserve(incomingCount);
    m_remoteOutgoing.reserve(outgoingCount);

    for (const auto& edge : edges) {
        const bool fromLocal = isNodeLocalRuntime(edge.fromNode);
        const bool toLocal = isNodeLocalRuntime(edge.toNode);
        if (fromLocal == toLocal) {
            continue;
        }

        const std::string channelId = edgeTransportId(edge);
        const uint16_t port = edgeTransportPort(channelId);

        if (fromLocal) {
            auto channel = std::make_unique<RemoteOutgoingChannel>();
            channel->id = channelId;
            channel->edge = edge;
            channel->targetHost = placementForNode(edge.toNode).runtimeHost;
            channel->port = port;
            m_remoteOutgoingById[channelId] = channel.get();
            m_remoteOutgoing.push_back(std::move(channel));
            continue;
        }

        auto channel = std::make_unique<RemoteIncomingChannel>();
        channel->id = channelId;
        channel->edge = edge;
        channel->sourceHost = placementForNode(edge.fromNode).runtimeHost;
        channel->port = port;
        if (channel->server.listen(port, "0.0.0.0") != 0) {
            LOG_ERROR("Failed to open transparent runtime transport listener on port " + std::to_string(port));
            teardown();
            return false;
        }
        m_remoteIncoming.push_back(std::move(channel));
    }

    if (!m_remoteIncoming.empty() || !m_remoteOutgoing.empty()) {
        LOG_INFO("Transparent runtime transport active: " + std::to_string(m_remoteOutgoing.size()) + " outgoing and " + std::to_string(m_remoteIncoming.size()) + " incoming channel(s)");
    }

    return true;
}

void EdgeBridge::teardown()
{
    for (auto& channel : m_remoteIncoming) {
        if (channel) {
            channel->server.closeSocket();
        }
    }
    for (auto& channel : m_remoteOutgoing) {
        if (channel) {
            channel->client.closeSocket();
        }
    }

    m_remoteIncoming.clear();
    m_remoteOutgoing.clear();
    m_remoteOutgoingById.clear();
    m_receivedRemoteChannels.clear();
}

void EdgeBridge::beginFrame()
{
    m_receivedRemoteChannels.clear();
}

bool EdgeBridge::isNodeLocalRuntime(const std::string& nodeId) const
{
    return placementForNode(nodeId).local;
}

size_t EdgeBridge::localNodeCount(const std::vector<NodePtr>& nodes) const
{
    size_t count = 0;
    for (const auto& node : nodes) {
        if (node && isNodeLocalRuntime(node->id())) {
            ++count;
        }
    }
    return count;
}

bool EdgeBridge::receiveOneRemoteInput(std::map<std::string, FrameContext>& contexts, std::map<std::string, size_t>& receivedInputs)
{
    for (const auto& channel : m_remoteIncoming) {
        if (!channel) {
            continue;
        }
        if (m_receivedRemoteChannels.find(channel->id) != m_receivedRemoteChannels.end()) {
            continue;
        }

        if (!channel->server.isConnected()) {
            if (channel->server.acceptClient() != 0) {
                continue;
            }
        }

        FrameContext incoming;
        if (channel->server.receiveFrameContext(incoming) != 0) {
            channel->server.closeClient();
            continue;
        }

        auto& targetContext = contexts[channel->edge.toNode];
        targetContext.mergeFrom(incoming);
        receivedInputs[channel->edge.toNode]++;
        m_receivedRemoteChannels.insert(channel->id);
        return true;
    }

    return false;
}

bool EdgeBridge::sendRemoteContext(const EdgeConfig& edge, const FrameContext& context)
{
    const bool fromLocal = isNodeLocalRuntime(edge.fromNode);
    const bool toLocal = isNodeLocalRuntime(edge.toNode);
    if (!fromLocal || toLocal) {
        return false;
    }

    const std::string channelId = edgeTransportId(edge);
    const auto channelIt = m_remoteOutgoingById.find(channelId);
    if (channelIt == m_remoteOutgoingById.end() || channelIt->second == nullptr) {
        return true;
    }

    auto& channel = *channelIt->second;
    auto& client = channel.client;

    if (!client.isConnected()) {
        if (client.open(channel.targetHost, channel.port) != 0) {
            LOG_WARNING("Transparent transport connect failed: " + channel.targetHost + ":" + std::to_string(channel.port));
            return true;
        }
    }

    const auto keys = scopedKeys(context, edge.fromNode);
    if (client.sendFrameContext(context, keys) == 0) {
        return true;
    }

    client.closeSocket();
    if (client.open(channel.targetHost, channel.port) != 0) {
        return true;
    }
    client.sendFrameContext(context, keys);
    return true;
}
