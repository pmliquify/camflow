// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "WebServer.hpp"

#include "core/Logger.hpp"
#include "image/ImageBuffer.hpp"

#include <arpa/inet.h>
#include <algorithm>
#include <any>
#include <array>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

namespace
{

std::string jsonEscape(const std::string& value)
{
    std::string result;
    for (char c : value) {
        if (c == '"') {
            result += "\\\"";
        } else if (c == '\\') {
            result += "\\\\";
        } else if (c == '\n') {
            result += "\\n";
        } else if (c == '\r') {
            result += "\\r";
        } else {
            result += c;
        }
    }
    return result;
}

std::string trim(const std::string& text)
{
    size_t start = 0;
    while (start < text.size() && (text[start] == ' ' || text[start] == '\t' || text[start] == '\r' || text[start] == '\n')) {
        ++start;
    }
    size_t end = text.size();
    while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r' || text[end - 1] == '\n')) {
        --end;
    }
    return text.substr(start, end - start);
}

std::string formatVerboseBody(const std::string& text, int verbosity)
{
    if (verbosity >= 3 || text.size() <= 80) {
        return text;
    }
    return text.substr(0, 80) + "...";
}

bool isRuntimeStatusPath(const std::string& path)
{
    return path == "/api/runtime" || path.rfind("/api/runtime?", 0) == 0;
}

bool suppressStatusPollingLog(const std::string& method, const std::string& path, int verbosity)
{
    return verbosity == 3 && method == "GET" && isRuntimeStatusPath(path);
}

const char* logTypeName(LogType type)
{
    switch (type) {
    case LogType::Debug:
        return "debug";
    case LogType::Info:
        return "info";
    case LogType::Warning:
        return "warning";
    case LogType::Error:
        return "error";
    }
    return "info";
}

std::string headerValue(const std::string& request, const std::string& header)
{
    const std::string marker = "\r\n" + header + ":";
    size_t pos = request.find(marker);
    if (pos == std::string::npos) {
        if (request.rfind(header + ":", 0) == 0) {
            pos = 0;
        } else {
            return std::string();
        }
    } else {
        pos += 2;
    }
    size_t valueStart = request.find(':', pos);
    if (valueStart == std::string::npos) {
        return std::string();
    }
    ++valueStart;
    size_t valueEnd = request.find("\r\n", valueStart);
    if (valueEnd == std::string::npos) {
        return std::string();
    }
    return trim(request.substr(valueStart, valueEnd - valueStart));
}

bool hasToken(const std::string& headerLine, const std::string& token)
{
    std::string lower = headerLine;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    std::string needle = token;
    for (char& c : needle) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower.find(needle) != std::string::npos;
}

std::string keyScopeFromQualified(const std::string& key)
{
    const size_t dot = key.find('.');
    if (dot == std::string::npos) {
        return std::string();
    }
    return key.substr(0, dot);
}

std::string keyNameFromQualified(const std::string& key)
{
    const size_t dot = key.find('.');
    if (dot == std::string::npos || dot + 1 >= key.size()) {
        return key;
    }
    return key.substr(dot + 1);
}

std::string jsonStringField(const std::string& payload, const std::string& field)
{
    const std::string marker = "\"" + field + "\"";
    const size_t markerPos = payload.find(marker);
    if (markerPos == std::string::npos) {
        return std::string();
    }
    const size_t colonPos = payload.find(':', markerPos + marker.size());
    if (colonPos == std::string::npos) {
        return std::string();
    }
    const size_t valueStart = payload.find('"', colonPos + 1);
    if (valueStart == std::string::npos) {
        return std::string();
    }
    size_t valueEnd = valueStart + 1;
    while (valueEnd < payload.size()) {
        if (payload[valueEnd] == '"' && payload[valueEnd - 1] != '\\') {
            break;
        }
        ++valueEnd;
    }
    if (valueEnd >= payload.size() || valueEnd <= valueStart + 1) {
        return std::string();
    }
    return payload.substr(valueStart + 1, valueEnd - valueStart - 1);
}

std::string jsonValueToText(const std::any* value)
{
    if (value == nullptr) {
        return std::string();
    }

    if (value->type() == typeid(bool)) {
        return std::any_cast<bool>(*value) ? "true" : "false";
    }
    if (value->type() == typeid(int64_t)) {
        return std::to_string(std::any_cast<int64_t>(*value));
    }
    if (value->type() == typeid(double)) {
        std::ostringstream stream;
        stream << std::any_cast<double>(*value);
        return stream.str();
    }
    if (value->type() == typeid(std::string)) {
        return std::any_cast<std::string>(*value);
    }
    return "<unsupported>";
}

} // namespace

WebServer::WebServer(const NodeFactory& factory, std::string sourceNodeId) :
    m_factory(factory),
    m_sourceNodeId(std::move(sourceNodeId)),
    m_webInterface(m_sourceNodeId),
    m_restApiInterface(nullptr),
    m_running(false),
    m_serverSocket(-1)
{
}

WebServer::~WebServer()
{
    stop();
}

bool WebServer::start(uint16_t port, RuntimeController& controller, int requestVerbosity)
{
    if (m_running) {
        return true;
    }

    m_requestVerbosity = requestVerbosity;

    m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverSocket < 0) {
        LOG_ERROR("REST server socket creation failed");
        return false;
    }

    int reuse = 1;
    setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(m_serverSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        const int errorNumber = errno;
        std::cerr << "Could not start REST server on port " << port << ": " << std::strerror(errorNumber) << '\n';
        close(m_serverSocket);
        m_serverSocket = -1;
        return false;
    }
    if (listen(m_serverSocket, 16) < 0) {
        LOG_ERROR("REST server listen failed");
        close(m_serverSocket);
        m_serverSocket = -1;
        return false;
    }

    m_controller = &controller;
    m_restApiInterface = std::make_unique<RestApiInterface>(m_factory, *m_controller, !m_sourceNodeId.empty());
    if (!m_frameObserverRegistered) {
        m_controller->addFrameContextObserver([this](const std::string& nodeId, const FrameContext& context) { broadcastFrameContextEvent(nodeId, context); });
        m_frameObserverRegistered = true;
    }

    registerLogListener();

    m_running = true;
    {
        std::lock_guard<std::mutex> lock(m_framePushMutex);
        m_framePushExit = false;
        m_framePushPending = false;
    }
    m_framePushThread = std::thread(&WebServer::framePushLoop, this);
    m_thread = std::thread(&WebServer::run, this);
    std::cout << "REST server listening on port " << port << '\n';
    return true;
}

void WebServer::stop()
{
    m_running = false;
    unregisterLogListener();
    {
        std::lock_guard<std::mutex> lock(m_framePushMutex);
        m_framePushExit = true;
        m_framePushPending = false;
    }
    m_framePushCv.notify_all();

    if (m_serverSocket >= 0) {
        shutdown(m_serverSocket, SHUT_RDWR);
        close(m_serverSocket);
        m_serverSocket = -1;
    }

    {
        std::lock_guard<std::mutex> lock(m_websocketMutex);
        if (m_frameClientSocket >= 0) {
            shutdown(m_frameClientSocket, SHUT_RDWR);
            close(m_frameClientSocket);
            m_frameClientSocket = -1;
        }
        ++m_frameClientGeneration;
        m_frameStreamNodeFilter.clear();
        m_frameStreamImageKey.clear();
    }

    {
        std::lock_guard<std::mutex> lock(m_logClientMutex);
        for (int socket : m_logClientSockets) {
            shutdown(socket, SHUT_RDWR);
            close(socket);
        }
        m_logClientSockets.clear();
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }
    if (m_framePushThread.joinable()) {
        m_framePushThread.join();
    }

    m_restApiInterface.reset();
    m_controller = nullptr;
}

void WebServer::run()
{
    if (m_serverSocket < 0) {
        return;
    }

    while (m_running) {
        int clientSocket = accept(m_serverSocket, nullptr, nullptr);
        if (clientSocket < 0) {
            continue;
        }
        std::thread(&WebServer::handleClient, this, clientSocket).detach();
    }
}

void WebServer::handleClient(int clientSocket)
{
    char buffer[16384];
    const ssize_t length = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (length <= 0) {
        close(clientSocket);
        return;
    }
    buffer[length] = 0;

    const std::string request(buffer, static_cast<size_t>(length));
    std::istringstream stream(request);
    std::string method;
    std::string path;
    stream >> method >> path;

    if (m_requestVerbosity >= 1 && !suppressStatusPollingLog(method, path, m_requestVerbosity)) {
        LOG_INFO("WebServer request: " + method + " " + path);
    }

    if (method == "OPTIONS") {
        const std::string response = httpResponse(204, std::string(), "text/plain; charset=utf-8");
        send(clientSocket, response.c_str(), response.size(), 0);
        close(clientSocket);
        return;
    }

    if (path == "/ws/frame" && hasToken(headerValue(request, "Upgrade"), "websocket") && hasToken(headerValue(request, "Connection"), "upgrade")) {
        if (handleWebSocketUpgrade(clientSocket, request)) {
            websocketClientLoop(clientSocket);
            return;
        }
        close(clientSocket);
        return;
    }

    if (path == "/ws/logs" && hasToken(headerValue(request, "Upgrade"), "websocket") && hasToken(headerValue(request, "Connection"), "upgrade")) {
        if (handleLogWebSocketUpgrade(clientSocket, request)) {
            websocketLogLoop(clientSocket);
            return;
        }
        close(clientSocket);
        return;
    }

    std::string body;
    const size_t bodyStart = request.find("\r\n\r\n");
    if (bodyStart != std::string::npos) {
        body = request.substr(bodyStart + 4);
    }

    const std::string response = handleRequest(method, path, body);
    send(clientSocket, response.c_str(), response.size(), 0);
    close(clientSocket);
}

bool WebServer::handleWebSocketUpgrade(int clientSocket, const std::string& request)
{
    const std::string clientKey = headerValue(request, "Sec-WebSocket-Key");
    if (clientKey.empty()) {
        return false;
    }

    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n";
    response << "Upgrade: websocket\r\n";
    response << "Connection: Upgrade\r\n";
    response << "Sec-WebSocket-Accept: " << websocketAcceptKey(clientKey) << "\r\n\r\n";

    const std::string text = response.str();
    if (send(clientSocket, text.c_str(), text.size(), 0) < 0) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_websocketMutex);
        if (m_frameClientSocket >= 0 && m_frameClientSocket != clientSocket) {
            shutdown(m_frameClientSocket, SHUT_RDWR);
            close(m_frameClientSocket);
        }
        ++m_frameClientGeneration;
        m_frameClientSocket = clientSocket;
        m_frameStreamNodeFilter = m_sourceNodeId;
        m_frameStreamImageKey.clear();
    }
    return true;
}

bool WebServer::handleLogWebSocketUpgrade(int clientSocket, const std::string& request)
{
    const std::string clientKey = headerValue(request, "Sec-WebSocket-Key");
    if (clientKey.empty()) {
        return false;
    }

    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n";
    response << "Upgrade: websocket\r\n";
    response << "Connection: Upgrade\r\n";
    response << "Sec-WebSocket-Accept: " << websocketAcceptKey(clientKey) << "\r\n\r\n";

    const std::string text = response.str();
    if (send(clientSocket, text.c_str(), text.size(), 0) < 0) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_logClientMutex);
        if (std::find(m_logClientSockets.begin(), m_logClientSockets.end(), clientSocket) == m_logClientSockets.end()) {
            m_logClientSockets.push_back(clientSocket);
        }
    }

    for (const auto& record : logger().history(200)) {
        if (!sendWebSocketTextFrame(clientSocket, logRecordJson(record))) {
            std::lock_guard<std::mutex> lock(m_logClientMutex);
            auto it = std::remove(m_logClientSockets.begin(), m_logClientSockets.end(), clientSocket);
            m_logClientSockets.erase(it, m_logClientSockets.end());
            return false;
        }
    }

    return true;
}

void WebServer::websocketClientLoop(int clientSocket)
{
    auto readExact = [clientSocket](uint8_t* dst, size_t count) -> bool {
        size_t offset = 0;
        while (offset < count) {
            const ssize_t received = recv(clientSocket, dst + offset, count - offset, 0);
            if (received <= 0) {
                return false;
            }
            offset += static_cast<size_t>(received);
        }
        return true;
    };

    while (m_running) {
        uint8_t header[2];
        if (!readExact(header, sizeof(header))) {
            break;
        }

        const uint8_t opcode = static_cast<uint8_t>(header[0] & 0x0f);
        const bool masked = (header[1] & 0x80u) != 0;
        uint64_t payloadLength = static_cast<uint64_t>(header[1] & 0x7fu);
        if (payloadLength == 126u) {
            uint8_t ext[2];
            if (!readExact(ext, sizeof(ext))) {
                break;
            }
            payloadLength = static_cast<uint64_t>(ext[0] << 8u) | static_cast<uint64_t>(ext[1]);
        } else if (payloadLength == 127u) {
            uint8_t ext[8];
            if (!readExact(ext, sizeof(ext))) {
                break;
            }
            payloadLength = 0;
            for (size_t i = 0; i < 8; ++i) {
                payloadLength = (payloadLength << 8u) | ext[i];
            }
        }

        uint8_t maskingKey[4] = {0, 0, 0, 0};
        if (masked && !readExact(maskingKey, sizeof(maskingKey))) {
            break;
        }

        std::vector<uint8_t> payload(payloadLength);
        if (payloadLength > 0 && !readExact(payload.data(), static_cast<size_t>(payloadLength))) {
            break;
        }
        if (masked) {
            for (size_t i = 0; i < payload.size(); ++i) {
                payload[i] ^= maskingKey[i % 4];
            }
        }

        if (opcode == 0x8u) {
            break;
        }
        if (opcode == 0x9u) {
            std::vector<uint8_t> pong;
            pong.push_back(0x8Au);
            if (payload.size() <= 125) {
                pong.push_back(static_cast<uint8_t>(payload.size()));
                pong.insert(pong.end(), payload.begin(), payload.end());
                const ssize_t sent = send(clientSocket, reinterpret_cast<const char*>(pong.data()), pong.size(), MSG_NOSIGNAL);
                if (sent != static_cast<ssize_t>(pong.size())) {
                    break;
                }
            }
            continue;
        }
        if (opcode == 0x1u) {
            const std::string command(payload.begin(), payload.end());
            std::string commandName = command;
            std::string nodeId = m_sourceNodeId;
            std::string imageKey;

            if (!command.empty() && command.front() == '{') {
                const std::string parsedCommand = jsonStringField(command, "cmd");
                if (!parsedCommand.empty()) {
                    commandName = parsedCommand;
                }
                const std::string parsedNodeId = jsonStringField(command, "nodeId");
                if (!parsedNodeId.empty()) {
                    nodeId = parsedNodeId;
                }
                const std::string parsedImageKey = jsonStringField(command, "imageKey");
                if (!parsedImageKey.empty()) {
                    imageKey = parsedImageKey;
                }
            }

            if (commandName == "subscribe") {
                std::lock_guard<std::mutex> lock(m_websocketMutex);
                if (m_frameClientSocket == clientSocket) {
                    if (m_frameStreamNodeFilter != nodeId || m_frameStreamImageKey != imageKey) {
                        ++m_frameClientGeneration;
                    }
                    m_frameStreamNodeFilter = nodeId;
                    m_frameStreamImageKey = imageKey;
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_websocketMutex);
        if (m_frameClientSocket == clientSocket) {
            m_frameClientSocket = -1;
            ++m_frameClientGeneration;
            m_frameStreamNodeFilter.clear();
            m_frameStreamImageKey.clear();
        }
    }
    close(clientSocket);
}

void WebServer::websocketLogLoop(int clientSocket)
{
    auto readExact = [clientSocket](uint8_t* dst, size_t count) -> bool {
        size_t offset = 0;
        while (offset < count) {
            const ssize_t received = recv(clientSocket, dst + offset, count - offset, 0);
            if (received <= 0) {
                return false;
            }
            offset += static_cast<size_t>(received);
        }
        return true;
    };

    while (m_running) {
        uint8_t header[2];
        if (!readExact(header, sizeof(header))) {
            break;
        }

        const uint8_t opcode = static_cast<uint8_t>(header[0] & 0x0f);
        const bool masked = (header[1] & 0x80u) != 0;
        uint64_t payloadLength = static_cast<uint64_t>(header[1] & 0x7fu);
        if (payloadLength == 126u) {
            uint8_t ext[2];
            if (!readExact(ext, sizeof(ext))) {
                break;
            }
            payloadLength = static_cast<uint64_t>(ext[0] << 8u) | static_cast<uint64_t>(ext[1]);
        } else if (payloadLength == 127u) {
            uint8_t ext[8];
            if (!readExact(ext, sizeof(ext))) {
                break;
            }
            payloadLength = 0;
            for (size_t i = 0; i < 8; ++i) {
                payloadLength = (payloadLength << 8u) | ext[i];
            }
        }

        uint8_t maskingKey[4] = {0, 0, 0, 0};
        if (masked && !readExact(maskingKey, sizeof(maskingKey))) {
            break;
        }

        std::vector<uint8_t> payload(payloadLength);
        if (payloadLength > 0 && !readExact(payload.data(), static_cast<size_t>(payloadLength))) {
            break;
        }
        if (masked) {
            for (size_t i = 0; i < payload.size(); ++i) {
                payload[i] ^= maskingKey[i % 4];
            }
        }

        if (opcode == 0x8u) {
            break;
        }
        if (opcode == 0x9u) {
            std::vector<uint8_t> pong;
            pong.push_back(0x8Au);
            if (payload.size() <= 125) {
                pong.push_back(static_cast<uint8_t>(payload.size()));
                pong.insert(pong.end(), payload.begin(), payload.end());
                const ssize_t sent = send(clientSocket, reinterpret_cast<const char*>(pong.data()), pong.size(), MSG_NOSIGNAL);
                if (sent != static_cast<ssize_t>(pong.size())) {
                    break;
                }
            }
            continue;
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_logClientMutex);
        auto it = std::remove(m_logClientSockets.begin(), m_logClientSockets.end(), clientSocket);
        m_logClientSockets.erase(it, m_logClientSockets.end());
    }
    close(clientSocket);
}

std::string WebServer::handleRequest(const std::string& method, const std::string& path, const std::string& body)
{
    int statusCode = 404;
    std::string contentType = "application/json";
    std::string responseBody = "{\"error\":\"not found\"}";

    const bool restHandled = m_restApiInterface != nullptr && m_restApiInterface->tryHandle(method, path, body, responseBody, contentType, statusCode);

    const bool webHandled = m_webInterface.tryHandle(method, path, responseBody, contentType, statusCode);
    if (m_requestVerbosity >= 2 && !suppressStatusPollingLog(method, path, m_requestVerbosity) && (restHandled || webHandled) &&
        (method == "GET" || method == "POST" || method == "PUT" || method == "DELETE")) {
        if (!body.empty()) {
            LOG_INFO("WebServer request body: " + formatVerboseBody(body, m_requestVerbosity));
        }
        if (path.rfind("/api/", 0) == 0) {
            LOG_INFO("WebServer response: " + std::to_string(statusCode) + " " + formatVerboseBody(responseBody, m_requestVerbosity));
        }
    }

    if (webHandled || restHandled) {
        return httpResponse(statusCode, responseBody, contentType);
    }

    return httpResponse(404, responseBody, contentType);
}

std::string WebServer::frameContextEventJson(const std::string& nodeId, const FrameContext& context) const
{
    const std::vector<std::string> keys = context.keys();

    std::ostringstream json;
    json << "{";
    json << "\"nodeId\":\"" << jsonEscape(nodeId) << "\",";
    json << "\"images\":[";
    bool firstImage = true;
    for (const auto& key : keys) {
        const std::string scope = keyScopeFromQualified(key);
        const std::string name = keyNameFromQualified(key);
        if (name != "image") {
            continue;
        }
        const ImageBuffer* image = context.get<ImageBuffer>(scope, name);
        if (image == nullptr) {
            continue;
        }

        if (!firstImage) {
            json << ",";
        }
        firstImage = false;
        json << "{";
        json << "\"key\":\"" << jsonEscape(key) << "\",";
        json << "\"width\":" << image->width() << ",";
        json << "\"height\":" << image->height() << ",";
        json << "\"formatId\":" << static_cast<uint32_t>(image->format()) << ",";
        json << "\"bitsPerPixel\":" << bitsPerPixel(image->format()) << ",";
        json << "\"sequence\":" << image->sequence() << ",";
        json << "\"timestampNs\":" << image->timestampNs();
        json << "}";
    }
    json << "],";

    json << "\"values\":[";
    bool firstValue = true;
    for (const auto& key : keys) {
        const std::string scope = keyScopeFromQualified(key);
        const std::string name = keyNameFromQualified(key);
        if (name == "image") {
            continue;
        }

        const std::any* value = context.valueAny(scope, name);
        if (value == nullptr || value->type() == typeid(ImageBuffer)) {
            continue;
        }

        std::string typeName = "unsupported";
        if (value->type() == typeid(bool)) {
            typeName = "bool";
        } else if (value->type() == typeid(int64_t)) {
            typeName = "int";
        } else if (value->type() == typeid(double)) {
            typeName = "double";
        } else if (value->type() == typeid(std::string)) {
            typeName = "string";
        }

        if (!firstValue) {
            json << ",";
        }
        firstValue = false;
        json << "{";
        json << "\"key\":\"" << jsonEscape(key) << "\",";
        json << "\"type\":\"" << typeName << "\",";
        json << "\"value\":\"" << jsonEscape(jsonValueToText(value)) << "\"";
        json << "}";
    }
    json << "],";

    json << "\"keys\":[";
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i) {
            json << ",";
        }
        json << "\"" << jsonEscape(keys[i]) << "\"";
    }
    json << "]}";
    return json.str();
}

void WebServer::broadcastFrameContextEvent(const std::string& nodeId, const FrameContext& context)
{
    int client = -1;
    uint64_t clientGeneration = 0;
    std::string nodeFilter;
    std::string imageFilter;
    {
        std::lock_guard<std::mutex> lock(m_websocketMutex);
        client = m_frameClientSocket;
        clientGeneration = m_frameClientGeneration;
        nodeFilter = m_frameStreamNodeFilter;
        imageFilter = m_frameStreamImageKey;
    }
    if (client < 0) {
        return;
    }

    if (!nodeFilter.empty() && nodeId != nodeFilter) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_framePushMutex);
        if (m_framePushPending) {
            return;
        }
    }

    const ImageBuffer* selectedImage = nullptr;
    const std::vector<std::string> keys = context.keys();
    if (!imageFilter.empty()) {
        if (imageFilter.find('.') != std::string::npos) {
            const std::string scope = keyScopeFromQualified(imageFilter);
            const std::string name = keyNameFromQualified(imageFilter);
            if (context.valueAny(scope, name) != nullptr) {
                selectedImage = context.get<ImageBuffer>(scope, name);
            }
        } else {
            for (const auto& key : keys) {
                const std::string scope = keyScopeFromQualified(key);
                const std::string name = keyNameFromQualified(key);
                if (name != imageFilter) {
                    continue;
                }
                if (context.valueAny(scope, name) == nullptr) {
                    continue;
                }
                selectedImage = context.get<ImageBuffer>(scope, name);
                if (selectedImage != nullptr) {
                    break;
                }
            }
        }
    }
    if (selectedImage == nullptr) {
        for (const auto& key : keys) {
            const std::string scope = keyScopeFromQualified(key);
            const std::string name = keyNameFromQualified(key);
            if (name != "image") {
                continue;
            }
            if (context.valueAny(scope, name) == nullptr) {
                continue;
            }
            selectedImage = context.get<ImageBuffer>(scope, name);
            if (selectedImage != nullptr) {
                break;
            }
        }
    }
    if (selectedImage == nullptr || selectedImage->data() == nullptr || selectedImage->size() == 0) {
        return;
    }

    std::vector<uint8_t> binaryPayload;
    binaryPayload.resize(48 + selectedImage->size());

    auto writeU16Le = [&binaryPayload](size_t offset, uint16_t value) {
        binaryPayload[offset + 0] = static_cast<uint8_t>(value & 0xff);
        binaryPayload[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xff);
    };
    auto writeU32Le = [&binaryPayload](size_t offset, uint32_t value) {
        binaryPayload[offset + 0] = static_cast<uint8_t>(value & 0xff);
        binaryPayload[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xff);
        binaryPayload[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xff);
        binaryPayload[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xff);
    };
    auto writeU64Le = [&binaryPayload](size_t offset, uint64_t value) {
        for (size_t i = 0; i < 8; ++i) {
            binaryPayload[offset + i] = static_cast<uint8_t>((value >> (8 * i)) & 0xff);
        }
    };

    writeU32Le(0, 0x31464d43u);
    writeU16Le(4, 1u);
    writeU16Le(6, 0u);
    writeU32Le(8, static_cast<uint32_t>(selectedImage->width()));
    writeU32Le(12, static_cast<uint32_t>(selectedImage->height()));
    writeU32Le(16, static_cast<uint32_t>(selectedImage->stride()));
    writeU32Le(20, static_cast<uint32_t>(selectedImage->format()));
    writeU16Le(24, static_cast<uint16_t>(bitsPerPixel(selectedImage->format())));
    writeU16Le(26, static_cast<uint16_t>(selectedImage->bitShift()));
    writeU64Le(28, selectedImage->sequence());
    writeU64Le(36, selectedImage->timestampNs());
    writeU32Le(44, static_cast<uint32_t>(selectedImage->size()));

    std::memcpy(binaryPayload.data() + 48, selectedImage->data(), selectedImage->size());
    const std::string contextPayload = frameContextEventJson(nodeId, context);

    {
        std::lock_guard<std::mutex> lock(m_framePushMutex);
        m_pendingFramePush.clientSocket = client;
        m_pendingFramePush.clientGeneration = clientGeneration;
        m_pendingFramePush.contextPayload = contextPayload;
        m_pendingFramePush.binaryPayload = std::move(binaryPayload);
        m_framePushPending = true;
    }
    m_framePushCv.notify_one();
}

std::string WebServer::logRecordJson(const Logger::LogRecord& record) const
{
    std::ostringstream json;
    json << "{";
    json << "\"source\":\"" << jsonEscape(record.source) << "\",";
    json << "\"type\":\"" << jsonEscape(logTypeName(record.type)) << "\",";
    json << "\"file\":\"" << jsonEscape(record.file) << "\",";
    json << "\"line\":" << record.line << ",";
    json << "\"sourceTag\":\"" << jsonEscape(record.sourceTag) << "\",";
    json << "\"message\":\"" << jsonEscape(record.message) << "\",";
    json << "\"rendered\":\"" << jsonEscape(record.rendered) << "\",";
    json << "\"timestampMs\":" << record.timestampMs;
    json << "}";
    return json.str();
}

void WebServer::broadcastLogRecord(const Logger::LogRecord& record)
{
    std::vector<int> sockets;
    {
        std::lock_guard<std::mutex> lock(m_logClientMutex);
        sockets = m_logClientSockets;
    }

    if (sockets.empty()) {
        return;
    }

    const std::string payload = logRecordJson(record);
    std::vector<int> failedSockets;
    for (int socket : sockets) {
        if (!sendWebSocketTextFrame(socket, payload)) {
            failedSockets.push_back(socket);
        }
    }

    if (!failedSockets.empty()) {
        std::lock_guard<std::mutex> lock(m_logClientMutex);
        m_logClientSockets.erase(std::remove_if(m_logClientSockets.begin(), m_logClientSockets.end(),
                                                [&failedSockets](int socket) { return std::find(failedSockets.begin(), failedSockets.end(), socket) != failedSockets.end(); }),
                                 m_logClientSockets.end());
    }
}

void WebServer::registerLogListener()
{
    if (m_logListenerRegistered) {
        return;
    }

    m_logListenerId = logger().addListener([this](const Logger::LogRecord& record) {
        if (m_running) {
            broadcastLogRecord(record);
        }
    });
    m_logListenerRegistered = true;
}

void WebServer::unregisterLogListener()
{
    if (!m_logListenerRegistered) {
        return;
    }

    logger().removeListener(m_logListenerId);
    m_logListenerRegistered = false;
    m_logListenerId = 0;
}

void WebServer::framePushLoop()
{
    while (true) {
        PendingFramePush push;
        {
            std::unique_lock<std::mutex> lock(m_framePushMutex);
            m_framePushCv.wait(lock, [this] { return m_framePushExit || m_framePushPending; });
            if (m_framePushExit) {
                return;
            }
            push = std::move(m_pendingFramePush);
            m_pendingFramePush = PendingFramePush{};
            m_framePushPending = false;
        }

        if (push.clientSocket < 0 || push.contextPayload.empty() || push.binaryPayload.empty()) {
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(m_websocketMutex);
            if (m_frameClientSocket != push.clientSocket || m_frameClientGeneration != push.clientGeneration) {
                continue;
            }
        }

        if (!sendWebSocketTextFrame(push.clientSocket, push.contextPayload) || !sendWebSocketBinaryFrame(push.clientSocket, push.binaryPayload)) {
            std::lock_guard<std::mutex> lock(m_websocketMutex);
            if (m_frameClientSocket == push.clientSocket && m_frameClientGeneration == push.clientGeneration) {
                m_frameClientSocket = -1;
                ++m_frameClientGeneration;
                m_frameStreamNodeFilter.clear();
                m_frameStreamImageKey.clear();
                shutdown(push.clientSocket, SHUT_RDWR);
                close(push.clientSocket);
            }
        }
    }
}

bool WebServer::sendWebSocketTextFrame(int clientSocket, const std::string& payload)
{
    std::vector<uint8_t> frame;
    frame.push_back(0x81);
    const size_t size = payload.size();
    if (size <= 125) {
        frame.push_back(static_cast<uint8_t>(size));
    } else if (size <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((size >> 8) & 0xff));
        frame.push_back(static_cast<uint8_t>(size & 0xff));
    } else {
        frame.push_back(127);
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.push_back(static_cast<uint8_t>((static_cast<uint64_t>(size) >> shift) & 0xff));
        }
    }
    frame.insert(frame.end(), payload.begin(), payload.end());
    const ssize_t sent = send(clientSocket, reinterpret_cast<const char*>(frame.data()), frame.size(), MSG_NOSIGNAL);
    return sent == static_cast<ssize_t>(frame.size());
}

bool WebServer::sendWebSocketBinaryFrame(int clientSocket, const std::vector<uint8_t>& payload)
{
    std::vector<uint8_t> frame;
    frame.push_back(0x82);
    const size_t size = payload.size();
    if (size <= 125) {
        frame.push_back(static_cast<uint8_t>(size));
    } else if (size <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((size >> 8) & 0xff));
        frame.push_back(static_cast<uint8_t>(size & 0xff));
    } else {
        frame.push_back(127);
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.push_back(static_cast<uint8_t>((static_cast<uint64_t>(size) >> shift) & 0xff));
        }
    }
    frame.insert(frame.end(), payload.begin(), payload.end());
    const ssize_t sent = send(clientSocket, reinterpret_cast<const char*>(frame.data()), frame.size(), MSG_NOSIGNAL);
    return sent == static_cast<ssize_t>(frame.size());
}

std::string WebServer::websocketAcceptKey(const std::string& clientKey)
{
    return base64Encode(sha1(clientKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
}

std::vector<uint8_t> WebServer::sha1(const std::string& text)
{
    auto leftRotate = [](uint32_t value, uint32_t bits) { return (value << bits) | (value >> (32 - bits)); };

    std::vector<uint8_t> bytes(text.begin(), text.end());
    const uint64_t originalBitLength = static_cast<uint64_t>(bytes.size()) * 8ull;
    bytes.push_back(0x80);
    while ((bytes.size() % 64) != 56) {
        bytes.push_back(0x00);
    }
    for (int i = 7; i >= 0; --i) {
        bytes.push_back(static_cast<uint8_t>((originalBitLength >> (i * 8)) & 0xff));
    }

    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    for (size_t chunk = 0; chunk < bytes.size(); chunk += 64) {
        std::array<uint32_t, 80> w{};
        for (size_t i = 0; i < 16; ++i) {
            const size_t base = chunk + i * 4;
            w[i] = (static_cast<uint32_t>(bytes[base]) << 24) | (static_cast<uint32_t>(bytes[base + 1]) << 16) | (static_cast<uint32_t>(bytes[base + 2]) << 8) | static_cast<uint32_t>(bytes[base + 3]);
        }
        for (size_t i = 16; i < 80; ++i) {
            w[i] = leftRotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        uint32_t a = h0;
        uint32_t b = h1;
        uint32_t c = h2;
        uint32_t d = h3;
        uint32_t e = h4;

        for (size_t i = 0; i < 80; ++i) {
            uint32_t f = 0;
            uint32_t k = 0;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            const uint32_t temp = leftRotate(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = leftRotate(b, 30);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    std::vector<uint8_t> digest(20);
    auto writeWord = [&](uint32_t value, size_t offset) {
        digest[offset + 0] = static_cast<uint8_t>((value >> 24) & 0xff);
        digest[offset + 1] = static_cast<uint8_t>((value >> 16) & 0xff);
        digest[offset + 2] = static_cast<uint8_t>((value >> 8) & 0xff);
        digest[offset + 3] = static_cast<uint8_t>(value & 0xff);
    };
    writeWord(h0, 0);
    writeWord(h1, 4);
    writeWord(h2, 8);
    writeWord(h3, 12);
    writeWord(h4, 16);
    return digest;
}

std::string WebServer::base64Encode(const std::vector<uint8_t>& bytes)
{
    static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    encoded.reserve(((bytes.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i + 2 < bytes.size()) {
        const uint32_t value = (static_cast<uint32_t>(bytes[i]) << 16) | (static_cast<uint32_t>(bytes[i + 1]) << 8) | static_cast<uint32_t>(bytes[i + 2]);
        encoded.push_back(table[(value >> 18) & 0x3f]);
        encoded.push_back(table[(value >> 12) & 0x3f]);
        encoded.push_back(table[(value >> 6) & 0x3f]);
        encoded.push_back(table[value & 0x3f]);
        i += 3;
    }

    if (i < bytes.size()) {
        uint32_t value = static_cast<uint32_t>(bytes[i]) << 16;
        encoded.push_back(table[(value >> 18) & 0x3f]);
        if (i + 1 < bytes.size()) {
            value |= static_cast<uint32_t>(bytes[i + 1]) << 8;
            encoded.push_back(table[(value >> 12) & 0x3f]);
            encoded.push_back(table[(value >> 6) & 0x3f]);
            encoded.push_back('=');
        } else {
            encoded.push_back(table[(value >> 12) & 0x3f]);
            encoded.push_back('=');
            encoded.push_back('=');
        }
    }

    return encoded;
}

std::string WebServer::httpResponse(int statusCode, const std::string& body, const std::string& contentType) const
{
    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << " ";
    switch (statusCode) {
    case 200:
        response << "OK";
        break;
    case 204:
        response << "No Content";
        break;
    case 404:
        response << "Not Found";
        break;
    case 503:
        response << "Service Unavailable";
        break;
    default:
        response << "Error";
        break;
    }
    response << "\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    response << "Access-Control-Allow-Headers: Content-Type\r\n";
    response << "Cache-Control: no-store, no-cache, must-revalidate\r\n";
    response << "Pragma: no-cache\r\n";
    response << "Expires: 0\r\n";
    response << "Connection: close\r\n\r\n";
    response << body;
    return response.str();
}
