// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "ImageConverter.hpp"

#include "core/Logger.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{

bool canConvertViaBgr(PixelFormat format)
{
    switch (format) {
    case PixelFormat::Mono8:
    case PixelFormat::Mono10:
    case PixelFormat::Mono12:
    case PixelFormat::Mono14:
    case PixelFormat::Mono16:
    case PixelFormat::RG8:
    case PixelFormat::RG10:
    case PixelFormat::RG12:
    case PixelFormat::RG14:
    case PixelFormat::GR8:
    case PixelFormat::GR10:
    case PixelFormat::GR12:
    case PixelFormat::GR14:
    case PixelFormat::BG8:
    case PixelFormat::BG10:
    case PixelFormat::BG12:
    case PixelFormat::BG14:
    case PixelFormat::GB8:
    case PixelFormat::GB10:
    case PixelFormat::GB12:
    case PixelFormat::GB14:
    case PixelFormat::RG10P:
    case PixelFormat::RG12P:
    case PixelFormat::RG14P:
    case PixelFormat::GR10P:
    case PixelFormat::GR12P:
    case PixelFormat::GR14P:
    case PixelFormat::BG10P:
    case PixelFormat::BG12P:
    case PixelFormat::BG14P:
    case PixelFormat::GB10P:
    case PixelFormat::GB12P:
    case PixelFormat::GB14P:
    case PixelFormat::RGB888:
    case PixelFormat::BGR888:
    case PixelFormat::YUYV:
    case PixelFormat::NV12:
    case PixelFormat::Raw8:
    case PixelFormat::Raw10:
    case PixelFormat::Raw12:
    case PixelFormat::Raw16:
        return true;
    default:
        return false;
    }
}

uint8_t clampToU8(float value)
{
    return static_cast<uint8_t>(std::clamp(std::lround(value), 0l, 255l));
}

void bgrToYuvBt601(const cv::Vec3b& bgr, uint8_t& y, uint8_t& u, uint8_t& v)
{
    const float blue = static_cast<float>(bgr[0]);
    const float green = static_cast<float>(bgr[1]);
    const float red = static_cast<float>(bgr[2]);
    y = clampToU8(0.114f * blue + 0.587f * green + 0.299f * red);
    u = clampToU8(-0.168736f * red - 0.331264f * green + 0.5f * blue + 128.0f);
    v = clampToU8(0.5f * red - 0.418688f * green - 0.081312f * blue + 128.0f);
}

} // namespace

static int bayerCode(PixelFormat format)
{
    switch (unpackedFormat(format)) {
    case PixelFormat::RG8:
    case PixelFormat::RG10:
    case PixelFormat::RG12:
    case PixelFormat::RG14:
        return cv::COLOR_BayerRG2BGR;
    case PixelFormat::GR8:
    case PixelFormat::GR10:
    case PixelFormat::GR12:
    case PixelFormat::GR14:
        return cv::COLOR_BayerGR2BGR;
    case PixelFormat::BG8:
    case PixelFormat::BG10:
    case PixelFormat::BG12:
    case PixelFormat::BG14:
        return cv::COLOR_BayerBG2BGR;
    case PixelFormat::GB8:
    case PixelFormat::GB10:
    case PixelFormat::GB12:
    case PixelFormat::GB14:
        return cv::COLOR_BayerGB2BGR;
    default:
        return -1;
    }
}

static uint16_t readUnpacked16(const uint8_t* data, size_t index)
{
    const uint16_t* pixels = reinterpret_cast<const uint16_t*>(data);
    return pixels[index];
}

static uint16_t readPackedBits(const uint8_t* data, size_t bitOffset, int bits)
{
    uint32_t value = 0;
    for (int i = 0; i < bits; ++i) {
        size_t absoluteBit = bitOffset + static_cast<size_t>(i);
        size_t byteIndex = absoluteBit / 8;
        int bitIndex = static_cast<int>(absoluteBit % 8);
        if ((data[byteIndex] >> bitIndex) & 1) {
            value |= (1u << i);
        }
    }
    return static_cast<uint16_t>(value);
}

static bool makeMono16(const ImageBuffer& source, ImageBuffer& mono16)
{
    int bits = bitsPerPixel(source.format());
    if (bits <= 0) {
        return false;
    }
    mono16.allocate(source.width(), source.height(), source.width() * 2, unpackedFormat(source.format()));
    uint16_t* out = reinterpret_cast<uint16_t*>(mono16.data());

    for (uint32_t y = 0; y < source.height(); ++y) {
        const uint8_t* srcLine = source.data() + static_cast<size_t>(y) * source.stride();
        for (uint32_t x = 0; x < source.width(); ++x) {
            uint16_t value = 0;
            if (isPackedFormat(source.format())) {
                value = readPackedBits(srcLine, static_cast<size_t>(x) * bits, bits);
            } else if (bits <= 8) {
                value = srcLine[x];
            } else {
                value = readUnpacked16(srcLine, x);
            }
            out[static_cast<size_t>(y) * source.width() + x] = value;
        }
    }

    if (source.bitShift() > 0) {
        const double divisor = static_cast<double>(1u << std::min<uint8_t>(source.bitShift(), 15));
        cv::Mat mono16Mat(static_cast<int>(source.height()), static_cast<int>(source.width()), CV_16UC1, mono16.data(), static_cast<size_t>(mono16.stride()));
        cv::divide(mono16Mat, divisor, mono16Mat);
    }

    mono16.setSequence(source.sequence());
    mono16.setTimestampNs(source.timestampNs());
    return true;
}

int ImageConverter::cost(PixelFormat sourceFormat, PixelFormat destinationFormat) const
{
    if (sourceFormat == destinationFormat) {
        return 0;
    }
    if (destinationFormat == PixelFormat::Mono8 && canConvertViaBgr(sourceFormat)) {
        return 10;
    }
    if (destinationFormat == PixelFormat::BGR888 && canConvertViaBgr(sourceFormat)) {
        return 10;
    }
    if (destinationFormat == PixelFormat::RGB888 && canConvertViaBgr(sourceFormat)) {
        return 11;
    }
    if ((destinationFormat == PixelFormat::YUYV || destinationFormat == PixelFormat::NV12) && canConvertViaBgr(sourceFormat)) {
        return 12;
    }
    return -1;
}

bool ImageConverter::convert(const ImageBuffer& source, ImageBuffer& destination, PixelFormat destinationFormat)
{
    if (source.format() == destinationFormat) {
        destination.assign(source.data(), source.size(), source.width(), source.height(), source.stride(), source.format());
        destination.setSequence(source.sequence());
        destination.setTimestampNs(source.timestampNs());
        return true;
    }
    if (destinationFormat == PixelFormat::Mono8) {
        return convertToMono8(source, destination);
    }
    if (destinationFormat == PixelFormat::BGR888) {
        return convertToBgr888(source, destination);
    }
    if (destinationFormat == PixelFormat::YUYV) {
        return convertToYuyv(source, destination);
    }
    if (destinationFormat == PixelFormat::NV12) {
        return convertToNv12(source, destination);
    }
    if (destinationFormat == PixelFormat::RGB888) {
        ImageBuffer bgr;
        if (!convertToBgr888(source, bgr)) {
            return false;
        }
        destination.allocate(source.width(), source.height(), source.width() * 3, PixelFormat::RGB888);
        cv::Mat src(bgr.height(), bgr.width(), CV_8UC3, bgr.data(), bgr.stride());
        cv::Mat dst(destination.height(), destination.width(), CV_8UC3, destination.data(), destination.stride());
        cv::cvtColor(src, dst, cv::COLOR_BGR2RGB);
        destination.setSequence(source.sequence());
        destination.setTimestampNs(source.timestampNs());
        return true;
    }
    return false;
}

bool ImageConverter::convertToMono8(const ImageBuffer& source, ImageBuffer& destination)
{
    if (source.format() == PixelFormat::BGR888 || source.format() == PixelFormat::RGB888) {
        int code = source.format() == PixelFormat::BGR888 ? cv::COLOR_BGR2GRAY : cv::COLOR_RGB2GRAY;
        cv::Mat src(source.height(), source.width(), CV_8UC3, const_cast<uint8_t*>(source.data()), source.stride());
        destination.allocate(source.width(), source.height(), source.width(), PixelFormat::Mono8);
        cv::Mat dst(destination.height(), destination.width(), CV_8UC1, destination.data(), destination.stride());
        cv::cvtColor(src, dst, code);
        destination.setSequence(source.sequence());
        destination.setTimestampNs(source.timestampNs());
        return true;
    }
    if (source.format() == PixelFormat::YUYV || source.format() == PixelFormat::NV12) {
        ImageBuffer bgr;
        if (!convertToBgr888(source, bgr)) {
            return false;
        }
        return convertToMono8(bgr, destination);
    }

    ImageBuffer mono16;
    if (!makeMono16(source, mono16)) {
        return false;
    }
    int bits = bitsPerPixel(source.format());
    int shift = bits > 8 ? bits - 8 : 0;
    destination.allocate(source.width(), source.height(), source.width(), PixelFormat::Mono8);
    const uint16_t* src = reinterpret_cast<const uint16_t*>(mono16.data());
    for (uint32_t y = 0; y < source.height(); ++y) {
        uint8_t* dstLine = destination.data() + static_cast<size_t>(y) * destination.stride();
        for (uint32_t x = 0; x < source.width(); ++x) {
            dstLine[x] = static_cast<uint8_t>(src[static_cast<size_t>(y) * source.width() + x] >> shift);
        }
    }
    destination.setSequence(source.sequence());
    destination.setTimestampNs(source.timestampNs());
    return true;
}

bool ImageConverter::convertToBgr888(const ImageBuffer& source, ImageBuffer& destination)
{
    if (source.format() == PixelFormat::RGB888) {
        destination.allocate(source.width(), source.height(), source.width() * 3, PixelFormat::BGR888);
        cv::Mat src(source.height(), source.width(), CV_8UC3, const_cast<uint8_t*>(source.data()), source.stride());
        cv::Mat dst(destination.height(), destination.width(), CV_8UC3, destination.data(), destination.stride());
        cv::cvtColor(src, dst, cv::COLOR_RGB2BGR);
        destination.setSequence(source.sequence());
        destination.setTimestampNs(source.timestampNs());
        return true;
    }
    if (source.format() == PixelFormat::Mono8) {
        destination.allocate(source.width(), source.height(), source.width() * 3, PixelFormat::BGR888);
        cv::Mat src(source.height(), source.width(), CV_8UC1, const_cast<uint8_t*>(source.data()), source.stride());
        cv::Mat dst(destination.height(), destination.width(), CV_8UC3, destination.data(), destination.stride());
        cv::cvtColor(src, dst, cv::COLOR_GRAY2BGR);
        destination.setSequence(source.sequence());
        destination.setTimestampNs(source.timestampNs());
        return true;
    }
    if (source.format() == PixelFormat::Mono10 || source.format() == PixelFormat::Mono12 || source.format() == PixelFormat::Mono14 || source.format() == PixelFormat::Mono16) {
        ImageBuffer mono8;
        if (!convertToMono8(source, mono8)) {
            return false;
        }
        return convertToBgr888(mono8, destination);
    }
    if (isBayerFormat(source.format())) {
        ImageBuffer mono8;
        if (!convertToMono8(source, mono8)) {
            return false;
        }
        destination.allocate(source.width(), source.height(), source.width() * 3, PixelFormat::BGR888);
        cv::Mat src(mono8.height(), mono8.width(), CV_8UC1, mono8.data(), mono8.stride());
        cv::Mat dst(destination.height(), destination.width(), CV_8UC3, destination.data(), destination.stride());
        int code = bayerCode(source.format());
        if (code < 0) {
            return false;
        }
        cv::cvtColor(src, dst, code);
        destination.setSequence(source.sequence());
        destination.setTimestampNs(source.timestampNs());
        return true;
    }
    if (source.format() == PixelFormat::YUYV) {
        destination.allocate(source.width(), source.height(), source.width() * 3, PixelFormat::BGR888);
        cv::Mat src(source.height(), source.width(), CV_8UC2, const_cast<uint8_t*>(source.data()), source.stride());
        cv::Mat dst(destination.height(), destination.width(), CV_8UC3, destination.data(), destination.stride());
        cv::cvtColor(src, dst, cv::COLOR_YUV2BGR_YUY2);
        destination.setSequence(source.sequence());
        destination.setTimestampNs(source.timestampNs());
        return true;
    }
    if (source.format() == PixelFormat::NV12) {
        const size_t ySize = static_cast<size_t>(source.stride()) * source.height();
        if (source.size() < ySize) {
            return false;
        }
        destination.allocate(source.width(), source.height(), source.width() * 3, PixelFormat::BGR888);
        cv::Mat yPlane(source.height(), source.width(), CV_8UC1, const_cast<uint8_t*>(source.data()), source.stride());
        cv::Mat uvPlane(source.height() / 2, source.width() / 2, CV_8UC2, const_cast<uint8_t*>(source.data()) + ySize, source.stride());
        cv::Mat dst(destination.height(), destination.width(), CV_8UC3, destination.data(), destination.stride());
        cv::cvtColorTwoPlane(yPlane, uvPlane, dst, cv::COLOR_YUV2BGR_NV12);
        destination.setSequence(source.sequence());
        destination.setTimestampNs(source.timestampNs());
        return true;
    }
    if (source.format() == PixelFormat::Raw8 || source.format() == PixelFormat::Raw10 || source.format() == PixelFormat::Raw12 || source.format() == PixelFormat::Raw16) {
        ImageBuffer mono8;
        if (!convertToMono8(source, mono8)) {
            return false;
        }
        return convertToBgr888(mono8, destination);
    }
    return false;
}

bool ImageConverter::convertToYuyv(const ImageBuffer& source, ImageBuffer& destination)
{
    ImageBuffer bgr;
    const ImageBuffer* input = &source;
    if (source.format() != PixelFormat::BGR888) {
        if (!convertToBgr888(source, bgr)) {
            return false;
        }
        input = &bgr;
    }

    if ((input->width() % 2) != 0) {
        LOG_ERROR("YUYV conversion requires even image width");
        return false;
    }

    destination.allocate(input->width(), input->height(), input->width() * 2, PixelFormat::YUYV);
    cv::Mat src(static_cast<int>(input->height()), static_cast<int>(input->width()), CV_8UC3, const_cast<uint8_t*>(input->data()), static_cast<size_t>(input->stride()));

    for (int y = 0; y < src.rows; ++y) {
        const cv::Vec3b* srcLine = src.ptr<cv::Vec3b>(y);
        uint8_t* dstLine = destination.data() + static_cast<size_t>(y) * destination.stride();
        for (int x = 0; x < src.cols; x += 2) {
            uint8_t y0 = 0;
            uint8_t u0 = 0;
            uint8_t v0 = 0;
            uint8_t y1 = 0;
            uint8_t u1 = 0;
            uint8_t v1 = 0;
            bgrToYuvBt601(srcLine[x], y0, u0, v0);
            bgrToYuvBt601(srcLine[x + 1], y1, u1, v1);
            dstLine[x * 2] = y0;
            dstLine[x * 2 + 1] = static_cast<uint8_t>((static_cast<int>(u0) + static_cast<int>(u1)) / 2);
            dstLine[x * 2 + 2] = y1;
            dstLine[x * 2 + 3] = static_cast<uint8_t>((static_cast<int>(v0) + static_cast<int>(v1)) / 2);
        }
    }

    destination.setSequence(source.sequence());
    destination.setTimestampNs(source.timestampNs());
    return true;
}

bool ImageConverter::convertToNv12(const ImageBuffer& source, ImageBuffer& destination)
{
    ImageBuffer bgr;
    const ImageBuffer* input = &source;
    if (source.format() != PixelFormat::BGR888) {
        if (!convertToBgr888(source, bgr)) {
            return false;
        }
        input = &bgr;
    }

    if ((input->width() % 2) != 0 || (input->height() % 2) != 0) {
        LOG_ERROR("NV12 conversion requires even image width and height");
        return false;
    }

    const uint32_t width = input->width();
    const uint32_t height = input->height();
    const size_t totalSize = static_cast<size_t>(width) * height * 3 / 2;
    std::vector<uint8_t> buffer(totalSize, 0);
    destination.assign(buffer.data(), buffer.size(), width, height, width, PixelFormat::NV12);

    cv::Mat src(static_cast<int>(height), static_cast<int>(width), CV_8UC3, const_cast<uint8_t*>(input->data()), static_cast<size_t>(input->stride()));
    uint8_t* yPlane = destination.data();
    uint8_t* uvPlane = destination.data() + static_cast<size_t>(width) * height;

    for (uint32_t y = 0; y < height; ++y) {
        const cv::Vec3b* srcLine = src.ptr<cv::Vec3b>(static_cast<int>(y));
        for (uint32_t x = 0; x < width; ++x) {
            uint8_t yValue = 0;
            uint8_t uValue = 0;
            uint8_t vValue = 0;
            bgrToYuvBt601(srcLine[x], yValue, uValue, vValue);
            yPlane[static_cast<size_t>(y) * width + x] = yValue;
        }
    }

    for (uint32_t y = 0; y < height; y += 2) {
        const cv::Vec3b* line0 = src.ptr<cv::Vec3b>(static_cast<int>(y));
        const cv::Vec3b* line1 = src.ptr<cv::Vec3b>(static_cast<int>(y + 1));
        for (uint32_t x = 0; x < width; x += 2) {
            uint8_t yDummy = 0;
            uint8_t u0 = 0;
            uint8_t v0 = 0;
            uint8_t u1 = 0;
            uint8_t v1 = 0;
            uint8_t u2 = 0;
            uint8_t v2 = 0;
            uint8_t u3 = 0;
            uint8_t v3 = 0;
            bgrToYuvBt601(line0[x], yDummy, u0, v0);
            bgrToYuvBt601(line0[x + 1], yDummy, u1, v1);
            bgrToYuvBt601(line1[x], yDummy, u2, v2);
            bgrToYuvBt601(line1[x + 1], yDummy, u3, v3);
            const size_t uvIndex = static_cast<size_t>(y / 2) * width + x;
            uvPlane[uvIndex] = static_cast<uint8_t>((static_cast<int>(u0) + static_cast<int>(u1) + static_cast<int>(u2) + static_cast<int>(u3)) / 4);
            uvPlane[uvIndex + 1] = static_cast<uint8_t>((static_cast<int>(v0) + static_cast<int>(v1) + static_cast<int>(v2) + static_cast<int>(v3)) / 4);
        }
    }

    destination.setSequence(source.sequence());
    destination.setTimestampNs(source.timestampNs());
    return true;
}
