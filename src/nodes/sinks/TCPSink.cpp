// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "TCPSink.hpp"
#include "core/Logger.hpp"
#include "image/ImageBuffer.hpp"

#include <cstdint>
#include <string>

TCPSink::TCPSink() = default;

TCPSink::~TCPSink()
{
    shutdown();
}

std::string TCPSink::typeName() const
{
    return "tcpsink";
}

std::string TCPSink::description() const
{
    return "Sends image frames to a TCP receiver.";
}

NodeSchema TCPSink::schema() const
{
    NodeSchema schema;
    schema.parameters = {{"ip", ParameterType::String, "Server address", std::string("127.0.0.1"), std::string(), std::string(), {}, true},
                         {"port", ParameterType::Int, "Server port", int64_t(9000), int64_t(1), int64_t(65535), {}, true},
                         {"reconnect", ParameterType::Bool, "Reconnect when the image socket connection is lost", true, false, true, {}, true}};
    schema.inputs = {NodeInputInfo{"image", "image", "Input image", false}};
    return schema;
}

bool TCPSink::init()
{
    return true;
}

bool TCPSink::start()
{
    const std::string ipValue = parameterString("ip", "127.0.0.1");
    const int portValue = static_cast<int>(parameterInt("port", 9000));
    return connectToServer(ipValue, portValue);
}

bool TCPSink::connectToServer(const std::string& ip, int port)
{
    if (m_socket.isConnected()) {
        return true;
    }
    LOG_INFO("Opening image socket connection to " + ip + ":" + std::to_string(port));
    if (m_socket.open(ip, static_cast<uint16_t>(port)) != 0) {
        LOG_WARNING("Could not connect to image socket server " + ip + ":" + std::to_string(port));
        return false;
    }
    return true;
}

bool TCPSink::process(FrameContext& context)
{
    const std::string ipValue = parameterString("ip", "127.0.0.1");
    const int portValue = static_cast<int>(parameterInt("port", 9000));
    const bool reconnect = parameterBool("reconnect", true);

    if (!m_socket.isConnected()) {
        if (!connectToServer(ipValue, portValue)) {
            return false;
        }
    }

    const auto bindings = resolveInputBindings("image", context);
    const ImageBuffer* image = nullptr;
    for (const auto& binding : bindings) {
        image = binding.first.empty() ? context.get<ImageBuffer>(binding.second) : context.get<ImageBuffer>(binding.first, binding.second);
        if (image != nullptr) {
            break;
        }
    }
    if (image == nullptr) {
        return false;
    }

    int sendResult = m_socket.sendImage(*image);

    while (sendResult != 0) {
        LOG_WARNING("Connection to image socket server lost");
        m_socket.closeSocket();
        if (!reconnect || !connectToServer(ipValue, portValue)) {
            return false;
        }
        sendResult = m_socket.sendImage(*image);
    }
    return true;
}

void TCPSink::stop()
{
    m_socket.closeSocket();
}

void TCPSink::shutdown()
{
    m_socket.closeSocket();
}
