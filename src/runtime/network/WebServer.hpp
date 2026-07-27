// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "core/Logger.hpp"
#include "core/RuntimeController.hpp"
#include "network/FrameContext.hpp"
#include "image/ImageBuffer.hpp"
#include "network/RestApiInterface.hpp"
#include "network/WebInterface.hpp"
#include "pipeline/NodeFactory.hpp"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief HTTP/WebSocket service that hosts the built-in web UI and REST API.
 *
 * WebServer owns the listening socket, accepts HTTP clients, performs the
 * WebSocket upgrades for frame streaming endpoints, and delegates plain HTTP
 * request handling to @ref WebInterface and @ref RestApiInterface.
 *
 * The application owns the @ref RuntimeController and passes it to @ref start,
 * so WebServer acts only as a transport/service layer and does not own runtime
 * state itself.
 *
 * Runtime behavior overview:
 * - `start()` creates and binds the listening socket synchronously so startup
 *   failures are reported immediately.
 * - the accept loop runs on a background thread created by @ref start.
 * - each client connection is handled on a detached worker thread.
 * - websocket clients subscribe once and receive frame-context metadata and
 *   matching raw frame packets over the same stream.
 * - frame payloads are sourced from FrameContext observer callbacks registered
 *   on the application-owned @ref RuntimeController.
 *
 * @see WebInterface
 * @see RestApiInterface
 * @see RuntimeController
 */
class WebServer
{
public:
    /** @brief Creates the service with the optional source node used by the UI frame endpoints. */
    explicit WebServer(const NodeFactory& factory, std::string sourceNodeId = std::string());

    /** @brief Stops the service if active and releases all sockets. */
    ~WebServer();

    /** @brief Starts the HTTP/WebSocket service on @p port using the application-owned controller. */
    bool start(uint16_t port, RuntimeController& controller, int requestVerbosity = 0);

    /** @brief Stops the service and joins the background accept thread. */
    void stop();

private:
    struct PendingFramePush
    {
        int clientSocket = -1;
        uint64_t clientGeneration = 0;
        std::string contextPayload;
        std::vector<uint8_t> binaryPayload;
    };

    void run(uint16_t port);
    void handleClient(int clientSocket);
    std::string handleRequest(const std::string& method, const std::string& path, const std::string& body);
    std::string frameContextEventJson(const std::string& nodeId, const FrameContext& context) const;
    bool handleWebSocketUpgrade(int clientSocket, const std::string& request);
    bool handleLogWebSocketUpgrade(int clientSocket, const std::string& request);
    void websocketClientLoop(int clientSocket);
    void websocketLogLoop(int clientSocket);
    void broadcastFrameContextEvent(const std::string& nodeId, const FrameContext& context);
    void broadcastLogRecord(const Logger::LogRecord& record);
    void framePushLoop();
    std::string logRecordJson(const Logger::LogRecord& record) const;
    void registerLogListener();
    void unregisterLogListener();

    static bool sendWebSocketTextFrame(int clientSocket, const std::string& payload);
    static bool sendWebSocketBinaryFrame(int clientSocket, const std::vector<uint8_t>& payload);
    static std::string websocketAcceptKey(const std::string& clientKey);
    static std::string base64Encode(const std::vector<uint8_t>& bytes);
    static std::vector<uint8_t> sha1(const std::string& text);

    std::string httpResponse(int statusCode, const std::string& body, const std::string& contentType = "application/json") const;

    const NodeFactory& m_factory;
    RuntimeController* m_controller = nullptr;
    std::string m_sourceNodeId;
    WebInterface m_webInterface;
    std::unique_ptr<RestApiInterface> m_restApiInterface;
    std::atomic_bool m_running;
    std::thread m_thread;
    std::thread m_framePushThread;
    int m_serverSocket;
    int m_frameClientSocket = -1;
    uint64_t m_frameClientGeneration = 0;
    std::string m_frameStreamNodeFilter;
    std::string m_frameStreamImageKey;
    std::vector<int> m_logClientSockets;
    mutable std::mutex m_websocketMutex;
    std::mutex m_logClientMutex;
    std::mutex m_framePushMutex;
    std::condition_variable m_framePushCv;
    bool m_framePushExit = false;
    bool m_framePushPending = false;
    PendingFramePush m_pendingFramePush;
    bool m_frameObserverRegistered = false;
    bool m_logListenerRegistered = false;
    size_t m_logListenerId = 0;
    int m_requestVerbosity = 0;
};