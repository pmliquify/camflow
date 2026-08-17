// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "V4L2Device.hpp"

#include <cerrno>
#include <fcntl.h>
#include <map>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

namespace
{

std::string fourccToString(uint32_t fourcc)
{
    std::string value(4, ' ');
    value[0] = static_cast<char>(fourcc & 0xffu);
    value[1] = static_cast<char>((fourcc >> 8) & 0xffu);
    value[2] = static_cast<char>((fourcc >> 16) & 0xffu);
    value[3] = static_cast<char>((fourcc >> 24) & 0xffu);
    return value;
}

} // namespace

V4L2Device::V4L2Device() :
    m_fd(-1),
    m_ownsFd(true),
    m_name(),
    m_streaming(false),
    m_format(),
    m_buffers(),
    m_nextBufferIndex(0),
    m_heldDequeuedBufferIndex(-1)
{
    m_format = {};
}

V4L2Device::~V4L2Device()
{
    close();
}

bool V4L2Device::open(const std::string& path, int flags)
{
    close();
    m_fd = ::open(path.c_str(), flags, 0);
    if (m_fd < 0) {
        m_name.clear();
        return false;
    }
    m_ownsFd = true;
    m_name = path;
    return true;
}

bool V4L2Device::attachBorrowed(int fd, const std::string& name)
{
    close();
    if (fd < 0) {
        return false;
    }
    m_fd = fd;
    m_ownsFd = false;
    m_name = name;
    return true;
}

void V4L2Device::close()
{
    stopCapture();
    if (m_ownsFd && m_fd >= 0) {
        ::close(m_fd);
    }
    m_fd = -1;
    m_ownsFd = true;
    m_name.clear();
}

bool V4L2Device::isOpen() const
{
    return m_fd >= 0;
}

int V4L2Device::fd() const
{
    return m_fd;
}

const std::string& V4L2Device::name() const
{
    return m_name;
}

bool V4L2Device::queryCapability(v4l2_capability& capability) const
{
    if (m_fd < 0) {
        return false;
    }
    return ::ioctl(m_fd, VIDIOC_QUERYCAP, &capability) == 0;
}

std::vector<V4L2Device::SupportedFormat> V4L2Device::enumerateSupportedFormats() const
{
    std::vector<SupportedFormat> formats;
    if (m_fd < 0) {
        return formats;
    }

    std::map<std::string, SupportedFormat> uniqueFormats;
    for (uint32_t type : {V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE}) {
        v4l2_fmtdesc format{};
        format.type = type;
        for (uint32_t index = 0;; ++index) {
            format.index = index;
            if (::ioctl(m_fd, VIDIOC_ENUM_FMT, &format) != 0) {
                break;
            }

            SupportedFormat supported;
            supported.fourcc = format.pixelformat;
            supported.fourccName = fourccToString(format.pixelformat);
            supported.description = reinterpret_cast<const char*>(format.description);
            uniqueFormats.emplace(supported.fourccName, supported);
        }
    }

    for (const auto& item : uniqueFormats) {
        formats.push_back(item.second);
    }
    return formats;
}

bool V4L2Device::supportsStreamingCapture() const
{
    v4l2_capability capability{};
    if (!queryCapability(capability)) {
        return false;
    }
    return (capability.capabilities & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING)) != 0;
}

bool V4L2Device::getFormat(v4l2_format& format) const
{
    if (m_fd < 0) {
        return false;
    }

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (::ioctl(m_fd, VIDIOC_G_FMT, &format) == 0) {
        return true;
    }

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    return ::ioctl(m_fd, VIDIOC_G_FMT, &format) == 0;
}

bool V4L2Device::setFormat(v4l2_format& format) const
{
    if (m_fd < 0) {
        return false;
    }
    return ::ioctl(m_fd, VIDIOC_S_FMT, &format) == 0;
}

bool V4L2Device::setSelection(v4l2_selection& selection) const
{
    if (m_fd < 0) {
        return false;
    }
    return ::ioctl(m_fd, VIDIOC_S_SELECTION, &selection) == 0;
}

bool V4L2Device::requestBuffers(v4l2_requestbuffers& request) const
{
    if (m_fd < 0) {
        return false;
    }
    return ::ioctl(m_fd, VIDIOC_REQBUFS, &request) == 0;
}

bool V4L2Device::queryBuffer(v4l2_buffer& buffer) const
{
    if (m_fd < 0) {
        return false;
    }
    return ::ioctl(m_fd, VIDIOC_QUERYBUF, &buffer) == 0;
}

bool V4L2Device::queueBuffer(v4l2_buffer& buffer) const
{
    if (m_fd < 0) {
        return false;
    }
    return ::ioctl(m_fd, VIDIOC_QBUF, &buffer) == 0;
}

bool V4L2Device::dequeueBuffer(v4l2_buffer& buffer) const
{
    if (m_fd < 0) {
        return false;
    }
    return ::ioctl(m_fd, VIDIOC_DQBUF, &buffer) == 0;
}

bool V4L2Device::streamOn(uint32_t type) const
{
    if (m_fd < 0) {
        return false;
    }
    auto streamType = static_cast<int>(type);
    return ::ioctl(m_fd, VIDIOC_STREAMON, &streamType) == 0;
}

bool V4L2Device::streamOff(uint32_t type) const
{
    if (m_fd < 0) {
        return false;
    }
    auto streamType = static_cast<int>(type);
    return ::ioctl(m_fd, VIDIOC_STREAMOFF, &streamType) == 0;
}

bool V4L2Device::startCapture(uint32_t requestedBufferCount)
{
    stopCapture();

    if (!refreshFormat()) {
        return false;
    }

    const uint32_t planeCount = m_format.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ? m_format.fmt.pix_mp.num_planes : 1;
    if (!createMMapBuffers(m_format.type, requestedBufferCount, planeCount, m_buffers)) {
        return false;
    }
    for (unsigned int index = 0; index < m_buffers.size(); ++index) {
        if (!queueBuffer(m_buffers[index].buffer)) {
            stopCapture();
            return false;
        }
    }

    if (!streamOn(m_format.type)) {
        stopCapture();
        return false;
    }

    m_streaming = true;
    m_nextBufferIndex = 0;
    m_heldDequeuedBufferIndex = -1;
    return true;
}

void V4L2Device::stopCapture()
{
    if (m_heldDequeuedBufferIndex >= 0 && static_cast<size_t>(m_heldDequeuedBufferIndex) < m_buffers.size()) {
        queueBuffer(m_buffers[static_cast<size_t>(m_heldDequeuedBufferIndex)].buffer);
    }
    m_heldDequeuedBufferIndex = -1;

    if (m_streaming) {
        streamOff(m_format.type);
    }
    m_streaming = false;

    if (!m_buffers.empty()) {
        releaseMMapBuffers(m_format.type, m_buffers);
    }
    m_nextBufferIndex = 0;
}

bool V4L2Device::captureFrame(int timeoutUs, CaptureFrame& frame)
{
    frame = {};
    if (!m_streaming || m_buffers.empty()) {
        return false;
    }

    if (m_heldDequeuedBufferIndex >= 0) {
        const size_t heldIndex = static_cast<size_t>(m_heldDequeuedBufferIndex);
        if (heldIndex >= m_buffers.size() || !queueBuffer(m_buffers[heldIndex].buffer)) {
            return false;
        }
        m_heldDequeuedBufferIndex = -1;
    }

    if (waitReadable(timeoutUs) != 0) {
        return false;
    }

    const size_t bufferIndex = static_cast<size_t>(m_nextBufferIndex) % m_buffers.size();
    MappedBuffer& mapped = m_buffers[bufferIndex];
    if (!dequeueBuffer(mapped.buffer)) {
        return false;
    }
    if (mapped.buffer.index >= m_buffers.size()) {
        return false;
    }

    MappedBuffer& active = m_buffers[mapped.buffer.index];
    frame.data = active.ptrs.empty() ? nullptr : active.ptrs[0];
    if (frame.data == nullptr) {
        return false;
    }

    frame.sequence = active.buffer.sequence;
    frame.timestampNs = static_cast<uint64_t>(active.buffer.timestamp.tv_sec) * 1000000000ull + static_cast<uint64_t>(active.buffer.timestamp.tv_usec) * 1000ull;

    switch (m_format.type) {
    case V4L2_BUF_TYPE_VIDEO_CAPTURE:
        frame.size = m_format.fmt.pix.sizeimage;
        frame.width = m_format.fmt.pix.width;
        frame.height = m_format.fmt.pix.height;
        frame.stride = m_format.fmt.pix.bytesperline;
        frame.fourcc = m_format.fmt.pix.pixelformat;
        break;
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
        frame.size = m_format.fmt.pix_mp.plane_fmt[0].sizeimage;
        frame.width = m_format.fmt.pix_mp.width;
        frame.height = m_format.fmt.pix_mp.height;
        frame.stride = m_format.fmt.pix_mp.plane_fmt[0].bytesperline;
        frame.fourcc = m_format.fmt.pix_mp.pixelformat;
        break;
    default:
        return false;
    }

    m_heldDequeuedBufferIndex = static_cast<int>(active.buffer.index);
    m_nextBufferIndex = static_cast<unsigned int>((active.buffer.index + 1) % m_buffers.size());
    return true;
}

const v4l2_format& V4L2Device::activeFormat() const
{
    return m_format;
}

bool V4L2Device::isStreaming() const
{
    return m_streaming;
}

uint32_t V4L2Device::activeWidth() const
{
    switch (m_format.type) {
    case V4L2_BUF_TYPE_VIDEO_CAPTURE:
        return m_format.fmt.pix.width;
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
        return m_format.fmt.pix_mp.width;
    default:
        return 0;
    }
}

uint32_t V4L2Device::activeHeight() const
{
    switch (m_format.type) {
    case V4L2_BUF_TYPE_VIDEO_CAPTURE:
        return m_format.fmt.pix.height;
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
        return m_format.fmt.pix_mp.height;
    default:
        return 0;
    }
}

uint32_t V4L2Device::activeStride() const
{
    switch (m_format.type) {
    case V4L2_BUF_TYPE_VIDEO_CAPTURE:
        return m_format.fmt.pix.bytesperline;
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
        return m_format.fmt.pix_mp.plane_fmt[0].bytesperline;
    default:
        return 0;
    }
}

uint32_t V4L2Device::activeFourcc() const
{
    switch (m_format.type) {
    case V4L2_BUF_TYPE_VIDEO_CAPTURE:
        return m_format.fmt.pix.pixelformat;
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
        return m_format.fmt.pix_mp.pixelformat;
    default:
        return 0;
    }
}

bool V4L2Device::refreshFormat()
{
    m_format = {};
    return getFormat(m_format);
}

bool V4L2Device::createMMapBuffers(uint32_t bufferType, uint32_t requestedCount, uint32_t planeCount, std::vector<MappedBuffer>& buffers) const
{
    releaseMMapBuffers(bufferType, buffers);

    v4l2_requestbuffers request{};
    request.type = bufferType;
    request.memory = V4L2_MEMORY_MMAP;
    request.count = requestedCount;
    if (!requestBuffers(request)) {
        return false;
    }

    buffers.clear();
    buffers.resize(request.count);

    for (uint32_t index = 0; index < request.count; ++index) {
        MappedBuffer& item = buffers[index];
        item.buffer = {};
        item.buffer.type = bufferType;
        item.buffer.memory = V4L2_MEMORY_MMAP;
        item.buffer.index = index;

        const uint32_t activePlaneCount = bufferType == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ? planeCount : 1;
        item.ptrs.assign(activePlaneCount, nullptr);
        item.sizes.assign(activePlaneCount, 0);
        if (bufferType == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
            item.planes.assign(activePlaneCount, v4l2_plane{});
            item.buffer.length = activePlaneCount;
            item.buffer.m.planes = item.planes.data();
        }

        if (!queryBuffer(item.buffer)) {
            releaseMMapBuffers(bufferType, buffers);
            return false;
        }

        if (bufferType == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
            item.sizes[0] = item.buffer.length;
            item.ptrs[0] = static_cast<uint8_t*>(mmapBuffer(item.buffer.length, static_cast<off_t>(item.buffer.m.offset)));
            if (item.ptrs[0] == nullptr) {
                releaseMMapBuffers(bufferType, buffers);
                return false;
            }
        } else {
            for (uint32_t planeIndex = 0; planeIndex < activePlaneCount; ++planeIndex) {
                item.sizes[planeIndex] = item.planes[planeIndex].length;
                item.ptrs[planeIndex] = static_cast<uint8_t*>(mmapBuffer(item.planes[planeIndex].length, static_cast<off_t>(item.planes[planeIndex].m.mem_offset)));
                if (item.ptrs[planeIndex] == nullptr) {
                    releaseMMapBuffers(bufferType, buffers);
                    return false;
                }
            }
        }
    }

    return true;
}

void V4L2Device::releaseMMapBuffers(uint32_t bufferType, std::vector<MappedBuffer>& buffers) const
{
    for (MappedBuffer& item : buffers) {
        for (size_t planeIndex = 0; planeIndex < item.ptrs.size(); ++planeIndex) {
            if (item.ptrs[planeIndex] != nullptr) {
                munmapBuffer(item.ptrs[planeIndex], item.sizes[planeIndex]);
            }
        }
    }
    buffers.clear();

    v4l2_requestbuffers request{};
    request.type = bufferType;
    request.memory = V4L2_MEMORY_MMAP;
    request.count = 0;
    requestBuffers(request);
}

int V4L2Device::waitReadable(int timeoutUs) const
{
    if (m_fd < 0) {
        return -1;
    }

    fd_set set;
    FD_ZERO(&set);
    FD_SET(m_fd, &set);

    timeval timeout{};
    timeout.tv_sec = timeoutUs / 1000000;
    timeout.tv_usec = timeoutUs % 1000000;

    int result = ::select(m_fd + 1, &set, nullptr, nullptr, timeoutUs < 0 ? nullptr : &timeout);
    if (result > 0) {
        return 0;
    }
    if (result == 0) {
        return -2;
    }
    if (errno == EINTR) {
        return -2;
    }
    return -1;
}

void* V4L2Device::mmapBuffer(size_t length, off_t offset) const
{
    if (m_fd < 0) {
        return nullptr;
    }
    void* address = ::mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, offset);
    if (address == MAP_FAILED) {
        return nullptr;
    }
    return address;
}

bool V4L2Device::munmapBuffer(void* address, size_t length) const
{
    if (address == nullptr) {
        return true;
    }
    return ::munmap(address, length) == 0;
}