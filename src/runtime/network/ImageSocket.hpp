// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "image/ImageBuffer.hpp"
#include "network/FrameContext.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/// Control ID: camera exposure value.
#define CID_EXPOSURE 1
/// Control ID: analogue gain.
#define CID_GAIN 2
/// Control ID: black level offset.
#define CID_BLACKLEVEL 3
/// Control ID: target frame rate.
#define CID_FRAMERATE 4

/**
 * @brief Base class for CamFlow binary image protocol sockets.
 *
 * Socket provides the common state (file descriptor, connection flag) and
 * low-level helpers used by both @ref ImageSocketClient and @ref ImageSocketServer.
 * It handles partial-send and partial-receive loops to ensure complete transmission
 * of binary protocol messages over TCP.
 *
 * @see ImageSocketClient
 * @see ImageSocketServer
 */
class Socket
{
public:
    Socket();
    virtual ~Socket();

    /** @brief Returns @c true if the socket is currently connected. */
    bool isConnected() const;

    /** @brief Closes the socket and resets the connection flag.
     *  @return @c 0 on success; negative errno on failure. */
    int closeSocket();

protected:
    int m_socket;     ///< File descriptor of the socket.
    bool m_connected; ///< @c true while the connection is alive.

    /**
     * @brief Sends exactly @p size bytes from @p data, retrying on partial sends.
     * @return @c 0 on success; negative errno on failure.
     */
    int sendAll(int fd, const void* data, size_t size);

    /**
     * @brief Receives exactly @p size bytes into @p data, retrying on partial reads.
     * @return @c 0 on success; negative errno on failure or disconnection.
     */
    int receiveAll(int fd, void* data, size_t size, int flags = 0);
};

/**
 * @brief TCP client that connects to a CamFlow image receiver and sends frames.
 *
 * ImageSocketClient establishes an outgoing TCP connection to an @ref ImageSocketServer
 * and provides methods to transmit @ref ImageBuffer frames and @ref FrameContext
 * payloads using the CamFlow binary image protocol. It also implements the receiver
 * side of control messages (exposure, gain, etc.) sent by the server.
 *
 * @see TCPSink
 * @see ImageSocketServer
 */
class ImageSocketClient : public Socket
{
public:
    /**
     * @brief Opens a TCP connection to the given address and port.
     * @param address Destination hostname or IP address.
     * @param port    Destination TCP port.
     * @return @c 0 on success; negative errno on failure.
     */
    int open(const std::string& address, uint16_t port);

    /**
     * @brief Receives a control message from the server.
     *
     * Control messages carry a control ID (e.g. @ref CID_GAIN) and a 64-bit value.
     * This is used for server → client camera parameter feedback.
     *
     * @param id    Output: received control ID.
     * @param value Output: received control value.
     * @return @c 0 on success; negative errno on disconnection or error.
     */
    int receiveControl(uint32_t& id, uint64_t& value);

    /**
     * @brief Sends an @ref ImageBuffer as a framed binary message.
     * @param image Image to transmit.
     * @return @c 0 on success; negative errno on failure.
     */
    int sendImage(const ImageBuffer& image);

    /**
     * @brief Sends the complete @ref FrameContext as a serialised binary payload.
     * @param context Frame context to transmit.
     * @return @c 0 on success; negative errno on failure.
     */
    int sendFrameContext(const FrameContext& context, const std::vector<std::string>& keys = {});
};

/**
 * @brief TCP server that accepts a single CamFlow client and receives image frames.
 *
 * ImageSocketServer listens on a TCP port and accepts one client connection from an
 * @ref ImageSocketClient sender. It provides methods to receive @ref ImageBuffer frames
 * and to send control messages back to the client (for camera parameter control).
 *
 * Runtime tooling may use this class to receive frames from @ref TCPSink.
 *
 * @see ImageSocketClient
 */
class ImageSocketServer : public Socket
{
public:
    ImageSocketServer();
    ~ImageSocketServer() override;

    /**
     * @brief Binds and starts listening on the given port.
     * @param port        TCP port to listen on.
     * @param bindAddress Interface address to bind to (default: all interfaces).
     * @return @c 0 on success; negative errno on failure.
     */
    int listen(uint16_t port, const std::string& bindAddress = "0.0.0.0");

    /**
     * @brief Blocks until a client connects and accepts the connection.
     * @return @c 0 on success; negative errno on failure.
     */
    int acceptClient();

    /** @brief Closes the accepted client connection without stopping the listener.
     *  @return @c 0 on success. */
    int closeClient();

    /** @brief Closes both the client and the listening socket.
     *  @return @c 0 on success. */
    int closeSocket();

    /**
     * @brief Sends a control message to the connected client.
     * @param id    Control ID (e.g. @ref CID_EXPOSURE).
     * @param value Control value.
     * @return @c 0 on success; negative errno on failure.
     */
    int sendControl(uint32_t id, uint64_t value);

    /**
     * @brief Receives the next @ref ImageBuffer frame from the client.
     * @param image Output: filled with the received frame data.
     * @return @c 0 on success; negative errno on disconnection or error.
     */
    int receiveImage(ImageBuffer& image);

    /**
     * @brief Receives the next @ref FrameContext payload from the client.
     * @param context Output: receives transported keys and optional image payload.
     * @return @c 0 on success; negative errno on disconnection or error.
     */
    int receiveFrameContext(FrameContext& context);

private:
    int m_client;     ///< File descriptor of the accepted client socket.
    bool m_listening; ///< @c true while the server socket is bound and listening.
};
