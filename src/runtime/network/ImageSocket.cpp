// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "ImageSocket.hpp"

#include "core/Logger.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

struct ImageSocketHeader
{
    uint16_t width;
    uint16_t height;
    uint16_t bytesPerLine;
    uint32_t imageSize;
    uint32_t bytesUsed;
    uint32_t pixelFormat;
    uint32_t sequence;
    uint64_t timestamp;
    uint8_t numPlanes;
    uint16_t shift;
};

struct ControlSocketHeader
{
    uint32_t id;
    uint64_t value;
};

struct FrameContextSocketHeader
{
    char magic[8];
    uint16_t version;
    uint16_t reserved;
    uint32_t keyCount;
    uint32_t keysSize;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t pixelFormat;
    uint64_t sequence;
    uint64_t timestampNs;
    uint64_t imageSize;
};

namespace
{

std::string encodeScalarAny(const std::any* value)
{
    if (value == nullptr) {
        return std::string();
    }

    if (value->type() == typeid(bool)) {
        return std::string("b:") + (std::any_cast<bool>(*value) ? "1" : "0");
    }
    if (value->type() == typeid(int64_t)) {
        return "i:" + std::to_string(std::any_cast<int64_t>(*value));
    }
    if (value->type() == typeid(double)) {
        return "d:" + std::to_string(std::any_cast<double>(*value));
    }
    if (value->type() == typeid(std::string)) {
        return "s:" + std::any_cast<std::string>(*value);
    }
    return std::string();
}

bool decodeScalarAny(const std::string& text, std::any& value)
{
    if (text.size() < 2 || text[1] != ':') {
        return false;
    }

    const char kind = text[0];
    const std::string payload = text.substr(2);
    try {
        switch (kind) {
        case 'b':
            value = (payload == "1" || payload == "true" || payload == "on");
            return true;
        case 'i':
            value = static_cast<int64_t>(std::stoll(payload));
            return true;
        case 'd':
            value = std::stod(payload);
            return true;
        case 's':
            value = payload;
            return true;
        default:
            return false;
        }
    } catch (...) {
        return false;
    }
}

std::pair<std::string, std::string> splitQualified(const std::string& qualifiedKey)
{
    const size_t dot = qualifiedKey.find('.');
    if (dot == std::string::npos) {
        return {std::string(), qualifiedKey};
    }
    if (dot + 1 >= qualifiedKey.size()) {
        return {qualifiedKey.substr(0, dot), std::string()};
    }
    return {qualifiedKey.substr(0, dot), qualifiedKey.substr(dot + 1)};
}

void appendLine(std::string& text, const std::string& line)
{
    text += line;
    text.push_back('\n');
}

} // namespace

Socket::Socket() :
    m_socket(-1),
    m_connected(false)
{
}

Socket::~Socket()
{
    closeSocket();
}

bool Socket::isConnected() const
{
    return m_connected;
}

int Socket::closeSocket()
{
    if (m_socket >= 0) {
        ::shutdown(m_socket, SHUT_RDWR);
        ::close(m_socket);
    }
    m_socket = -1;
    m_connected = false;
    return 0;
}

int Socket::sendAll(int fd, const void* data, size_t size)
{
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    size_t sent = 0;
    while (sent < size) {
        ssize_t ret = ::send(fd, ptr + sent, size - sent, MSG_NOSIGNAL);
        if (ret <= 0) {
            return -1;
        }
        sent += static_cast<size_t>(ret);
    }
    return 0;
}

int Socket::receiveAll(int fd, void* data, size_t size, int flags)
{
    uint8_t* ptr = static_cast<uint8_t*>(data);
    size_t received = 0;
    while (received < size) {
        ssize_t ret = ::recv(fd, ptr + received, size - received, flags);
        if (ret <= 0) {
            return -1;
        }
        received += static_cast<size_t>(ret);
        flags &= ~MSG_PEEK;
    }
    return 0;
}

int ImageSocketClient::open(const std::string& address, uint16_t port)
{
    if (isConnected()) {
        return -1;
    }

    m_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < 0) {
        return -1;
    }

    hostent* server = ::gethostbyname(address.c_str());
    if (server == nullptr) {
        closeSocket();
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    std::memcpy(&addr.sin_addr.s_addr, server->h_addr, static_cast<size_t>(server->h_length));

    if (::connect(m_socket, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
        closeSocket();
        return -1;
    }

    m_connected = true;
    return 0;
}

int ImageSocketClient::receiveControl(uint32_t& id, uint64_t& value)
{
    ControlSocketHeader header{};
    ssize_t size = ::recv(m_socket, &header, sizeof(header), MSG_PEEK | MSG_DONTWAIT);
    if (size < static_cast<ssize_t>(sizeof(header))) {
        return -1;
    }
    if (receiveAll(m_socket, &header, sizeof(header), 0) != 0) {
        closeSocket();
        return -1;
    }
    id = header.id;
    value = header.value;
    return 0;
}

int ImageSocketClient::sendFrameContext(const FrameContext& context, const std::vector<std::string>& keys)
{
    if (!isConnected()) {
        return -1;
    }

    std::vector<std::string> selectedKeys = keys;
    if (selectedKeys.empty()) {
        selectedKeys = context.keys();
    }

    const ImageBuffer* image = nullptr;
    std::string imageKey;
    for (const auto& key : selectedKeys) {
        const std::any* value = context.valueAny(key);
        if (value != nullptr && value->type() == typeid(ImageBuffer)) {
            image = context.get<ImageBuffer>(key);
            if (image != nullptr) {
                imageKey = key;
                break;
            }
        }
    }

    if (image == nullptr) {
        for (const auto& key : context.keys()) {
            const std::any* value = context.valueAny(key);
            if (value == nullptr || value->type() != typeid(ImageBuffer)) {
                continue;
            }
            image = context.get<ImageBuffer>(key);
            if (image != nullptr) {
                imageKey = key;
                break;
            }
        }
    }

    std::string keyPayload;
    uint32_t keyCount = 0;

    if (!imageKey.empty()) {
        appendLine(keyPayload, std::string("I\t") + imageKey);
        keyCount += 1;
    }

    for (const auto& key : selectedKeys) {
        if (key == imageKey) {
            continue;
        }
        const std::string encoded = encodeScalarAny(context.valueAny(key));
        if (encoded.empty()) {
            continue;
        }
        appendLine(keyPayload, std::string("S\t") + key + "\t" + encoded);
        keyCount += 1;
    }

    FrameContextSocketHeader header{};
    std::memcpy(header.magic, "CFCTX001", 8);
    header.version = 1;
    header.keyCount = keyCount;
    header.keysSize = static_cast<uint32_t>(keyPayload.size());
    if (image != nullptr) {
        header.width = image->width();
        header.height = image->height();
        header.stride = image->stride();
        header.pixelFormat = static_cast<uint32_t>(image->format());
        header.sequence = image->sequence();
        header.timestampNs = image->timestampNs();
        header.imageSize = image->size();
    }

    if (sendAll(m_socket, &header, sizeof(header)) != 0) {
        return -1;
    }
    if (header.keysSize > 0) {
        if (sendAll(m_socket, keyPayload.data(), keyPayload.size()) != 0) {
            return -1;
        }
    }
    if (image != nullptr && image->size() > 0) {
        if (sendAll(m_socket, image->data(), image->size()) != 0) {
            return -1;
        }
    }
    return 0;
}

int ImageSocketClient::sendImage(const ImageBuffer& image)
{
    if (!isConnected()) {
        return -1;
    }

    ImageSocketHeader header{};
    header.width = static_cast<uint16_t>(image.width());
    header.height = static_cast<uint16_t>(image.height());
    header.bytesPerLine = static_cast<uint16_t>(image.stride());
    header.imageSize = static_cast<uint32_t>(image.size());
    header.bytesUsed = static_cast<uint32_t>(image.size());
    header.pixelFormat = static_cast<uint32_t>(pixelFormatToFourCC(image.format()));
    header.sequence = static_cast<uint32_t>(image.sequence());
    header.timestamp = image.timestampNs() / 1000000ull; // Convert to milliseconds
    header.numPlanes = 1;
    header.shift = image.bitShift();

    uint32_t planeSize = static_cast<uint32_t>(image.size());
    if (sendAll(m_socket, &header, sizeof(header)) != 0) {
        return -1;
    }
    if (sendAll(m_socket, &planeSize, sizeof(planeSize)) != 0) {
        return -1;
    }
    if (sendAll(m_socket, image.data(), image.size()) != 0) {
        return -1;
    }
    return 0;
}

ImageSocketServer::ImageSocketServer() :
    m_client(-1),
    m_listening(false)
{
}

ImageSocketServer::~ImageSocketServer()
{
    closeSocket();
}

int ImageSocketServer::listen(uint16_t port, const std::string& bindAddress)
{
    if (m_listening) {
        return -1;
    }

    m_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < 0) {
        return -1;
    }

    int enable = 1;
    ::setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(bindAddress.c_str());
    if (bindAddress == "0.0.0.0" || bindAddress.empty()) {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (::bind(m_socket, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        return -1;
    }
    if (::listen(m_socket, 1) < 0) {
        return -1;
    }

    m_listening = true;
    return 0;
}

int ImageSocketServer::acceptClient()
{
    if (!m_listening) {
        return -1;
    }
    int fd = ::accept(m_socket, nullptr, nullptr);
    if (fd < 0) {
        return -1;
    }
    m_client = fd;
    m_connected = true;
    return 0;
}

int ImageSocketServer::closeClient()
{
    if (m_client >= 0) {
        ::shutdown(m_client, SHUT_RDWR);
        ::close(m_client);
    }
    m_client = -1;
    m_connected = false;
    return 0;
}

int ImageSocketServer::closeSocket()
{
    closeClient();
    if (m_socket >= 0) {
        ::shutdown(m_socket, SHUT_RDWR);
        ::close(m_socket);
    }
    m_socket = -1;
    m_listening = false;
    return 0;
}

int ImageSocketServer::sendControl(uint32_t id, uint64_t value)
{
    if (!isConnected()) {
        return -1;
    }
    ControlSocketHeader header{};
    header.id = id;
    header.value = value;
    return sendAll(m_client, &header, sizeof(header));
}

int ImageSocketServer::receiveImage(ImageBuffer& image)
{
    if (!isConnected()) {
        return -1;
    }

    ImageSocketHeader header{};
    if (receiveAll(m_client, &header, sizeof(header), MSG_WAITALL) != 0) {
        closeClient();
        return -1;
    }

    if (header.numPlanes == 0) {
        closeClient();
        return -1;
    }

    uint32_t planeSize = 0;
    if (receiveAll(m_client, &planeSize, sizeof(planeSize), MSG_WAITALL) != 0) {
        closeClient();
        return -1;
    }

    image.allocate(header.width, header.height, header.bytesPerLine, static_cast<PixelFormat>(header.pixelFormat));
    if (planeSize > image.size()) {
        image.allocate(header.width, header.height, planeSize / header.height, static_cast<PixelFormat>(header.pixelFormat));
    }
    if (receiveAll(m_client, image.data(), planeSize, MSG_WAITALL) != 0) {
        closeClient();
        return -1;
    }
    image.setSequence(header.sequence);
    image.setBitShift(static_cast<uint8_t>(header.shift));
    image.setTimestampNs(header.timestamp * 1000000ull); // Convert from milliseconds to nanoseconds
    return 0;
}

int ImageSocketServer::receiveFrameContext(FrameContext& context)
{
    if (!isConnected()) {
        return -1;
    }

    FrameContextSocketHeader header{};
    if (receiveAll(m_client, &header, sizeof(header), MSG_WAITALL) != 0) {
        closeClient();
        return -1;
    }

    if (std::memcmp(header.magic, "CFCTX001", 8) != 0) {
        closeClient();
        return -1;
    }

    context.clear();

    std::string keyPayload;
    if (header.keysSize > 0) {
        keyPayload.resize(header.keysSize);
        if (receiveAll(m_client, keyPayload.data(), keyPayload.size(), MSG_WAITALL) != 0) {
            closeClient();
            return -1;
        }
    }

    std::string imageKey = "image";
    size_t lineStart = 0;
    while (lineStart < keyPayload.size()) {
        size_t lineEnd = keyPayload.find('\n', lineStart);
        if (lineEnd == std::string::npos) {
            lineEnd = keyPayload.size();
        }
        const std::string line = keyPayload.substr(lineStart, lineEnd - lineStart);
        lineStart = lineEnd + 1;
        if (line.empty()) {
            continue;
        }

        if (line.rfind("I\t", 0) == 0) {
            imageKey = line.substr(2);
            continue;
        }

        if (line.rfind("S\t", 0) == 0) {
            const size_t keySplit = line.find('\t', 2);
            if (keySplit == std::string::npos || keySplit + 1 >= line.size()) {
                continue;
            }
            const std::string key = line.substr(2, keySplit - 2);
            const std::string encodedValue = line.substr(keySplit + 1);
            std::any decoded;
            if (!decodeScalarAny(encodedValue, decoded)) {
                continue;
            }
            const auto parts = splitQualified(key);
            if (decoded.type() == typeid(bool)) {
                if (!parts.first.empty()) {
                    context.set(parts.first, parts.second, std::any_cast<bool>(decoded));
                } else {
                    context.set(parts.second, std::any_cast<bool>(decoded));
                }
            } else if (decoded.type() == typeid(int64_t)) {
                if (!parts.first.empty()) {
                    context.set(parts.first, parts.second, std::any_cast<int64_t>(decoded));
                } else {
                    context.set(parts.second, std::any_cast<int64_t>(decoded));
                }
            } else if (decoded.type() == typeid(double)) {
                if (!parts.first.empty()) {
                    context.set(parts.first, parts.second, std::any_cast<double>(decoded));
                } else {
                    context.set(parts.second, std::any_cast<double>(decoded));
                }
            } else if (decoded.type() == typeid(std::string)) {
                if (!parts.first.empty()) {
                    context.set(parts.first, parts.second, std::any_cast<std::string>(decoded));
                } else {
                    context.set(parts.second, std::any_cast<std::string>(decoded));
                }
            }
        }
    }

    if (header.imageSize > 0) {
        ImageBuffer image;
        image.allocate(header.width, header.height, header.stride, static_cast<PixelFormat>(header.pixelFormat));
        if (header.imageSize > image.size()) {
            closeClient();
            return -1;
        }
        if (receiveAll(m_client, image.data(), static_cast<size_t>(header.imageSize), MSG_WAITALL) != 0) {
            closeClient();
            return -1;
        }
        image.setSequence(header.sequence);
        image.setTimestampNs(header.timestampNs);

        const auto imageParts = splitQualified(imageKey);
        if (!imageParts.first.empty()) {
            context.set(imageParts.first, imageParts.second, std::move(image));
        } else {
            context.set(imageParts.second, std::move(image));
        }
    }

    return 0;
}
