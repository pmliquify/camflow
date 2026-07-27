// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "ImageBuffer.hpp"

ImageBuffer::ImageBuffer() :
    m_externalData(nullptr),
    m_externalSize(0),
    m_width(0),
    m_height(0),
    m_stride(0),
    m_format(PixelFormat::Unknown),
    m_bitShift(0),
    m_timestampNs(0),
    m_sequence(0)
{
}

void ImageBuffer::allocate(uint32_t width, uint32_t height, uint32_t stride, PixelFormat format)
{
    m_width = width;
    m_height = height;
    m_stride = stride;
    m_format = format;
    m_externalData = nullptr;
    m_externalSize = 0;
    m_externalOwner.reset();
    m_data.resize(static_cast<size_t>(stride) * static_cast<size_t>(height));
}

void ImageBuffer::assign(const uint8_t* data, size_t size, uint32_t width, uint32_t height, uint32_t stride, PixelFormat format)
{
    m_width = width;
    m_height = height;
    m_stride = stride;
    m_format = format;
    m_externalData = nullptr;
    m_externalSize = 0;
    m_externalOwner.reset();
    m_data.assign(data, data + size);
}

void ImageBuffer::wrapExternal(uint8_t* data, size_t size, uint32_t width, uint32_t height, uint32_t stride, PixelFormat format, std::shared_ptr<void> owner)
{
    m_width = width;
    m_height = height;
    m_stride = stride;
    m_format = format;
    m_data.clear();
    m_externalData = data;
    m_externalSize = size;
    m_externalOwner = std::move(owner);
}

uint8_t* ImageBuffer::data()
{
    if (m_externalData != nullptr) {
        return m_externalData;
    }
    return m_data.data();
}

const uint8_t* ImageBuffer::data() const
{
    if (m_externalData != nullptr) {
        return m_externalData;
    }
    return m_data.data();
}

size_t ImageBuffer::size() const
{
    if (m_externalData != nullptr) {
        return m_externalSize;
    }
    return m_data.size();
}

uint32_t ImageBuffer::width() const
{
    return m_width;
}

uint32_t ImageBuffer::height() const
{
    return m_height;
}

uint32_t ImageBuffer::stride() const
{
    return m_stride;
}

PixelFormat ImageBuffer::format() const
{
    return m_format;
}

uint8_t ImageBuffer::bitShift() const
{
    return m_bitShift;
}

void ImageBuffer::setBitShift(uint8_t bitShift)
{
    m_bitShift = bitShift;
}

void ImageBuffer::setTimestampNs(uint64_t timestampNs)
{
    m_timestampNs = timestampNs;
}

uint64_t ImageBuffer::timestampNs() const
{
    return m_timestampNs;
}

void ImageBuffer::setSequence(uint64_t sequence)
{
    m_sequence = sequence;
}

uint64_t ImageBuffer::sequence() const
{
    return m_sequence;
}