// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "network/ImageSocket.hpp"
#include "pipeline/Node.hpp"

#include <string>

/**
 * @brief Sink node that transmits the current image over a TCP connection.
 *
 * TCPSink connects to a remote @ref ImageSocketServer and sends the current
 * @ref ImageBuffer over TCP. If a `bitshift` value exists in the @ref FrameContext,
 * it is forwarded in the image header metadata.
 *
 * ### Parameters
 * | Name        | Type   | Description                                          |
 * |-------------|--------|------------------------------------------------------|
 * | `ip`        | string | Destination IP address or hostname.                  |
 * | `port`      | int    | Destination TCP port.                                |
 * | `reconnect` | bool   | If @c true, attempts to reconnect on lost connection.|
 *
 * At runtime these values are read from node parameters.

 * ### Inputs
 * | Name       | Type  | Description                                      |
 * |------------|-------|--------------------------------------------------|
 * | `image`    | image | Frame image to transmit.                         |
 * | `bitshift` | int   | Shift metadata for TCP header (`0..8`, default `0`). |
 *
 * ### Protocol
 * The binary framing protocol is defined in @ref ImageSocketClient. Each
 * transmission consists of the image header and pixel data for the current frame.
 *
 * @see ImageSocketClient
 */
class TCPSink : public Node
{
public:
    TCPSink();
    ~TCPSink() override;

    /** @brief Returns `"tcpsink"`. */
    std::string typeName() const override;

    /** @brief Returns a human-readable description of this sink. */
    std::string description() const override;

    /** @brief Returns the parameter schema with `ip`, `port` and `reconnect`. */
    NodeSchema schema() const override;

    /** @brief Performs lightweight setup before the node starts. */
    bool init() override;

    /** @brief Connects to the configured receiver before processing starts. */
    bool start() override;

    /**
     * @brief Sends the current frame over the TCP socket.
     *
     * Also forwards optional `bitshift` metadata from the frame context in the
     * image header shift field.
     *
     * @param context Per-frame data carrier with the image and metadata.
     * @return @c true if the frame was sent successfully; @c false on unrecoverable error.
     */
    bool process(FrameContext& context) override;

    /** @brief Closes the TCP connection. */
    void stop() override;

    /** @brief Closes the TCP connection. */
    void shutdown() override;

private:
    /** @brief Attempts to connect or reconnect to the configured server.
     *  @return @c true if the connection succeeded. */
    bool connectToServer(const std::string& ip, int port);

    ImageSocketClient m_socket; ///< Underlying TCP socket client.
};
