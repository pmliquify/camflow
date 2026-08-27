// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "PixelFormat.hpp"

#include <algorithm>
#include <map>

static std::string upper(const std::string& value)
{
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

static const std::map<std::string, PixelFormat>& formatMap()
{
    static const std::map<std::string, PixelFormat> formats = {
        {"GREY", PixelFormat::Mono8},    {"Y10 ", PixelFormat::Mono10},   {"Y12 ", PixelFormat::Mono12},   {"Y14 ", PixelFormat::Mono14}, {"Y16 ", PixelFormat::Mono16}, {"RGGB", PixelFormat::RG8},
        {"RG10", PixelFormat::RG10},     {"RG12", PixelFormat::RG12},     {"RG14", PixelFormat::RG14},     {"GRBG", PixelFormat::GR8},    {"BA10", PixelFormat::GR10},   {"BA12", PixelFormat::GR12},
        {"BA14", PixelFormat::GR14},     {"BGGR", PixelFormat::BG8},      {"BG10", PixelFormat::BG10},     {"BG12", PixelFormat::BG12},   {"BG14", PixelFormat::BG14},   {"GBRG", PixelFormat::GB8},
        {"GB10", PixelFormat::GB10},     {"GB12", PixelFormat::GB12},     {"GB14", PixelFormat::GB14},     {"RGB3", PixelFormat::RGB888}, {"BGR3", PixelFormat::BGR888}, {"YUYV", PixelFormat::YUYV},
        {"NV12", PixelFormat::NV12},     {"GREY", PixelFormat::Raw8},     {"Y10 ", PixelFormat::Raw10},    {"Y12 ", PixelFormat::Raw12},  {"Y16 ", PixelFormat::Raw16},  {"MONO8", PixelFormat::Mono8},
        {"MONO16", PixelFormat::Mono16}, {"RGB888", PixelFormat::RGB888}, {"BGR888", PixelFormat::BGR888}, {"RAW8", PixelFormat::Raw8},   {"RAW10", PixelFormat::Raw10}, {"RAW12", PixelFormat::Raw12},
        {"RAW16", PixelFormat::Raw16}};
    return formats;
}

std::string pixelFormatToString(PixelFormat format)
{
    switch (format) {
    case PixelFormat::Mono8:
        return "GREY";
    case PixelFormat::Mono10:
        return "Y10 ";
    case PixelFormat::Mono12:
        return "Y12 ";
    case PixelFormat::Mono14:
        return "Y14 ";
    case PixelFormat::Mono16:
        return "Y16 ";
    case PixelFormat::RG8:
        return "RGGB";
    case PixelFormat::RG10:
        return "RG10";
    case PixelFormat::RG12:
        return "RG12";
    case PixelFormat::RG14:
        return "RG14";
    case PixelFormat::GR8:
        return "GRBG";
    case PixelFormat::GR10:
        return "BA10";
    case PixelFormat::GR12:
        return "BA12";
    case PixelFormat::GR14:
        return "BA14";
    case PixelFormat::BG8:
        return "BGGR";
    case PixelFormat::BG10:
        return "BG10";
    case PixelFormat::BG12:
        return "BG12";
    case PixelFormat::BG14:
        return "BG14";
    case PixelFormat::GB8:
        return "GBRG";
    case PixelFormat::GB10:
        return "GB10";
    case PixelFormat::GB12:
        return "GB12";
    case PixelFormat::GB14:
        return "GB14";
    case PixelFormat::RG10P:
        return "pRAA";
    case PixelFormat::RG12P:
        return "pRCC";
    case PixelFormat::RG14P:
        return "pREE";
    case PixelFormat::GR10P:
        return "pgAA";
    case PixelFormat::GR12P:
        return "pgCC";
    case PixelFormat::GR14P:
        return "pgEE";
    case PixelFormat::BG10P:
        return "pBAA";
    case PixelFormat::BG12P:
        return "pBCC";
    case PixelFormat::BG14P:
        return "pBEE";
    case PixelFormat::GB10P:
        return "pGAA";
    case PixelFormat::GB12P:
        return "pGCC";
    case PixelFormat::GB14P:
        return "pGEE";
    case PixelFormat::RGB888:
        return "RGB3";
    case PixelFormat::BGR888:
        return "BGR3";
    case PixelFormat::YUYV:
        return "YUYV";
    case PixelFormat::NV12:
        return "NV12";
    case PixelFormat::Raw8:
        return "GREY";
    case PixelFormat::Raw10:
        return "Y10 ";
    case PixelFormat::Raw12:
        return "Y12 ";
    case PixelFormat::Raw16:
        return "Y16 ";
    case PixelFormat::Unknown:
    default:
        return "UNKN";
    }
}

PixelFormat pixelFormatFromString(const std::string& value)
{
    static const std::map<std::string, PixelFormat> packedBayerFormats = {{"pRAA", PixelFormat::RG10P}, {"pRCC", PixelFormat::RG12P}, {"pREE", PixelFormat::RG14P}, {"pgAA", PixelFormat::GR10P},
                                                                          {"pgCC", PixelFormat::GR12P}, {"pgEE", PixelFormat::GR14P}, {"pBAA", PixelFormat::BG10P}, {"pBCC", PixelFormat::BG12P},
                                                                          {"pBEE", PixelFormat::BG14P}, {"pGAA", PixelFormat::GB10P}, {"pGCC", PixelFormat::GB12P}, {"pGEE", PixelFormat::GB14P}};
    const auto packed = packedBayerFormats.find(value);
    if (packed != packedBayerFormats.end()) {
        return packed->second;
    }

    std::string key = upper(value);
    auto it = formatMap().find(key);
    if (it == formatMap().end()) {
        return PixelFormat::Unknown;
    }
    return it->second;
}

static uint32_t packFourCC(const std::string& value)
{
    if (value.size() < 4) {
        return 0;
    }
    return static_cast<uint32_t>(static_cast<unsigned char>(value[0])) | (static_cast<uint32_t>(static_cast<unsigned char>(value[1])) << 8) |
           (static_cast<uint32_t>(static_cast<unsigned char>(value[2])) << 16) | (static_cast<uint32_t>(static_cast<unsigned char>(value[3])) << 24);
}

uint32_t pixelFormatToFourCC(PixelFormat format)
{
    return packFourCC(pixelFormatToString(format));
}

PixelFormat pixelFormatFromFourCC(uint32_t fourcc, PixelFormat fallback)
{
    char value[5] = {static_cast<char>(fourcc & 0xff), static_cast<char>((fourcc >> 8) & 0xff), static_cast<char>((fourcc >> 16) & 0xff), static_cast<char>((fourcc >> 24) & 0xff), 0};
    PixelFormat parsed = pixelFormatFromString(value);
    if (parsed != PixelFormat::Unknown) {
        return parsed;
    }
    return fallback;
}

bool isPackedFormat(PixelFormat format)
{
    switch (format) {
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
        return true;
    default:
        return false;
    }
}

bool isBayerFormat(PixelFormat format)
{
    switch (format) {
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
        return true;
    default:
        return false;
    }
}

int bitsPerPixel(PixelFormat format)
{
    switch (format) {
    case PixelFormat::Mono8:
    case PixelFormat::RG8:
    case PixelFormat::GR8:
    case PixelFormat::BG8:
    case PixelFormat::GB8:
    case PixelFormat::Raw8:
        return 8;
    case PixelFormat::Mono10:
    case PixelFormat::RG10:
    case PixelFormat::GR10:
    case PixelFormat::BG10:
    case PixelFormat::GB10:
    case PixelFormat::RG10P:
    case PixelFormat::GR10P:
    case PixelFormat::BG10P:
    case PixelFormat::GB10P:
    case PixelFormat::Raw10:
        return 10;
    case PixelFormat::Mono12:
    case PixelFormat::RG12:
    case PixelFormat::GR12:
    case PixelFormat::BG12:
    case PixelFormat::GB12:
    case PixelFormat::RG12P:
    case PixelFormat::GR12P:
    case PixelFormat::BG12P:
    case PixelFormat::GB12P:
    case PixelFormat::Raw12:
        return 12;
    case PixelFormat::Mono14:
    case PixelFormat::RG14:
    case PixelFormat::GR14:
    case PixelFormat::BG14:
    case PixelFormat::GB14:
    case PixelFormat::RG14P:
    case PixelFormat::GR14P:
    case PixelFormat::BG14P:
    case PixelFormat::GB14P:
        return 14;
    case PixelFormat::Mono16:
    case PixelFormat::Raw16:
        return 16;
    case PixelFormat::RGB888:
    case PixelFormat::BGR888:
        return 24;
    default:
        return 0;
    }
}

PixelFormat unpackedFormat(PixelFormat format)
{
    switch (format) {
    case PixelFormat::RG10P:
        return PixelFormat::RG10;
    case PixelFormat::RG12P:
        return PixelFormat::RG12;
    case PixelFormat::RG14P:
        return PixelFormat::RG14;
    case PixelFormat::GR10P:
        return PixelFormat::GR10;
    case PixelFormat::GR12P:
        return PixelFormat::GR12;
    case PixelFormat::GR14P:
        return PixelFormat::GR14;
    case PixelFormat::BG10P:
        return PixelFormat::BG10;
    case PixelFormat::BG12P:
        return PixelFormat::BG12;
    case PixelFormat::BG14P:
        return PixelFormat::BG14;
    case PixelFormat::GB10P:
        return PixelFormat::GB10;
    case PixelFormat::GB12P:
        return PixelFormat::GB12;
    case PixelFormat::GB14P:
        return PixelFormat::GB14;
    default:
        return format;
    }
}
