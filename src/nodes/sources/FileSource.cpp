// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "FileSource.hpp"

#include "core/Logger.hpp"
#include "image/ImageBuffer.hpp"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fnmatch.h>
#include <fstream>
#include <iterator>
#include <regex>
#include <sstream>
#include <vector>

namespace
{

bool endsWithCaseInsensitive(const std::string& value, const std::string& suffix)
{
    if (value.size() < suffix.size()) {
        return false;
    }
    const size_t start = value.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i) {
        const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(value[start + i])));
        const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(suffix[i])));
        if (a != b) {
            return false;
        }
    }
    return true;
}

bool isRawFileExtension(const std::string& fileName)
{
    static const std::vector<std::string> extensions = {".raw",    ".bin",    ".grey",    ".rggb8",   ".gbrg8",   ".grbg8",   ".bggr8",   ".y10", ".y10p",   ".rggb10",
                                                        ".gbrg10", ".grbg10", ".bggr10",  ".rggb10p", ".gbrg10p", ".grbg10p", ".bggr10p", ".y12", ".y12p",   ".rggb12",
                                                        ".gbrg12", ".grbg12", ".bggr12",  ".rggb12p", ".gbrg12p", ".grbg12p", ".bggr12p", ".y14", ".rggb14", ".gbrg14",
                                                        ".grbg14", ".bggr14", ".rggb14p", ".gbrg14p", ".grbg14p", ".bggr14p", ".yuyv",    ".nv12"};
    for (const auto& ext : extensions) {
        if (endsWithCaseInsensitive(fileName, ext)) {
            return true;
        }
    }
    return false;
}

bool hasWildcardPattern(const std::string& value)
{
    return value.find_first_of("*?[") != std::string::npos;
}

std::optional<uint64_t> parseSequenceFromFileName(const std::string& fileName)
{
    const std::string stem = std::filesystem::path(fileName).stem().string();
    std::stringstream stream(stem);
    std::string token;
    while (std::getline(stream, token, '_')) {
        if (token.empty()) {
            continue;
        }
        const bool allDigits = std::all_of(token.begin(), token.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
        if (!allDigits) {
            continue;
        }
        // Skip likely datetime tokens created by FileSink (YYYYMMDD / HHMMSS).
        if (token.size() == 8 || token.size() == 6) {
            continue;
        }
        try {
            return static_cast<uint64_t>(std::stoull(token));
        } catch (...) {
            continue;
        }
    }
    return std::nullopt;
}

bool isRawPixelFormat(PixelFormat format)
{
    switch (format) {
    case PixelFormat::Mono8:
    case PixelFormat::RG8:
    case PixelFormat::GB8:
    case PixelFormat::GR8:
    case PixelFormat::BG8:
    case PixelFormat::Mono10:
    case PixelFormat::RG10:
    case PixelFormat::GB10:
    case PixelFormat::GR10:
    case PixelFormat::BG10:
    case PixelFormat::RG10P:
    case PixelFormat::GB10P:
    case PixelFormat::GR10P:
    case PixelFormat::BG10P:
    case PixelFormat::Mono12:
    case PixelFormat::Mono14:
    case PixelFormat::RG12:
    case PixelFormat::GB12:
    case PixelFormat::GR12:
    case PixelFormat::BG12:
    case PixelFormat::RG14:
    case PixelFormat::GB14:
    case PixelFormat::GR14:
    case PixelFormat::BG14:
    case PixelFormat::RG12P:
    case PixelFormat::GB12P:
    case PixelFormat::GR12P:
    case PixelFormat::BG12P:
    case PixelFormat::RG14P:
    case PixelFormat::GB14P:
    case PixelFormat::GR14P:
    case PixelFormat::BG14P:
    case PixelFormat::YUYV:
    case PixelFormat::NV12:
        return true;
    default:
        return false;
    }
}

uint32_t defaultStrideFor(PixelFormat format, uint32_t width)
{
    if (format == PixelFormat::YUYV) {
        return width * 2;
    }
    if (format == PixelFormat::NV12) {
        return width;
    }
    if (isPackedFormat(format)) {
        return static_cast<uint32_t>((static_cast<uint64_t>(width) * bitsPerPixel(format) + 7) / 8);
    }
    const int bpp = bitsPerPixel(format);
    if (bpp <= 8) {
        return width;
    }
    return width * 2;
}

bool isPackedMonoToken(const std::string& value)
{
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return normalized == "Y10P" || normalized == "Y12P";
}

bool fileNameContainsPackedMonoToken(const std::string& fileName)
{
    std::string normalized = fileName;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return normalized.find("Y10P") != std::string::npos || normalized.find("Y12P") != std::string::npos;
}

size_t expectedRawSize(PixelFormat format, uint32_t width, uint32_t height, uint32_t stride)
{
    (void)width;
    if (format == PixelFormat::NV12) {
        return static_cast<size_t>(stride) * height + static_cast<size_t>(stride) * (height / 2);
    }
    return static_cast<size_t>(stride) * height;
}

} // namespace

FileSource::FileSource() :
    m_fileIndex(0),
    m_sequence(0)
{
}

std::string FileSource::typeName() const
{
    return "filesrc";
}

std::string FileSource::description() const
{
    return "Reads raw, PNG and JPEG image files or streams image files from a directory.";
}

NodeSchema FileSource::schema() const
{
    NodeSchema schema;
    schema.parameters = {
        {"filename", ParameterType::String, "Input path. May contain directory, wildcard and extension (e.g. images/frame_*.raw)", std::string("images/*"), std::string(), std::string(), {}, true},
        {"width", ParameterType::Int, "Raw image width", int64_t(0), int64_t(0), int64_t(8192), {}, true},
        {"height", ParameterType::Int, "Raw image height", int64_t(0), int64_t(0), int64_t(8192), {}, true},
        {"stride", ParameterType::Int, "Raw line stride in bytes; 0 calculates a default", int64_t(0), int64_t(0), int64_t(8192), {}, true},
        {"bitshift", ParameterType::Int, "Bit shift metadata carried with the ImageBuffer for RAW conversion", int64_t(0), int64_t(0), int64_t(8), {}, true},
        {"format",
         ParameterType::Option,
         "Raw input format override when filename has no raw format metadata",
         std::string("auto"),
         std::string(),
         std::string(),
         {"auto",  "GREY", "RGGB", "GBRG", "GRBG", "BGGR", "Y10",  "Y10P", "RG10", "GB10", "BA10", "BG10", "Y12",  "Y12P", "RG12", "GB12",
          "BA12",  "BG12", "Y14",  "RG14", "GB14", "BA14", "BG14", "YUYV", "NV12", "pRAA", "pRCC", "pREE", "pgAA", "pgCC", "pgEE", "pBAA",
          "pBCC",  "pBEE", "pGAA", "pGCC", "pGEE"},
         true},
        {"repeat", ParameterType::Bool, "Repeat file sequence", false, false, true, {}, true}};
    schema.outputs = {NodeOutputInfo{"image", "image", "Decoded frame"}};
    return schema;
}

bool FileSource::init()
{
    return collectInputFiles();
}

bool FileSource::start()
{
    // Re-scan on every (re)start since files on disk may have changed while stopped.
    return collectInputFiles();
}

bool FileSource::onParameterChanged(const std::string& name, const ParameterValue&, const ParameterValue*, std::string& errorMessage)
{
    static const std::vector<std::string> collectionParameters = {"filename", "width", "height", "stride", "bitshift", "format"};
    if (std::find(collectionParameters.begin(), collectionParameters.end(), name) == collectionParameters.end()) {
        return true;
    }
    if (!collectInputFiles()) {
        errorMessage = "could not resolve any input file for the given filename";
        return false;
    }
    return true;
}

bool FileSource::process(FrameContext& context)
{
    const bool repeat = parameterBool("repeat", false);

    if (m_inputFiles.empty()) {
        LOG_ERROR("FileSource requires filename");
        return false;
    }

    if (m_fileIndex >= m_inputFiles.size()) {
        if (!repeat) {
            return false;
        }
        m_fileIndex = 0;
    }

    const std::string fileName = m_inputFiles[m_fileIndex++];
    bool ok = loadFile(fileName, context);
    return ok;
}

bool FileSource::collectInputFiles()
{
    m_inputFiles.clear();
    m_fileIndex = 0;

    const std::string configuredFileName = parameterString("filename", std::string("images/*"));

    auto addFromDirectory = [this](const std::string& directory, const std::string& wildcard) -> bool {
        std::error_code errorCode;
        if (!std::filesystem::is_directory(directory, errorCode)) {
            LOG_ERROR("FileSource directory does not exist: " + directory);
            return false;
        }

        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const std::string fileName = entry.path().filename().string();
            const char* wildcardPattern = wildcard.empty() ? "*" : wildcard.c_str();
            if (fnmatch(wildcardPattern, fileName.c_str(), 0) != 0) {
                continue;
            }
            const std::string path = entry.path().string();
            if (isSupportedImageFile(path)) {
                m_inputFiles.push_back(path);
            }
        }

        std::sort(m_inputFiles.begin(), m_inputFiles.end());
        LOG_INFO("FileSource found " + std::to_string(m_inputFiles.size()) + " image files in " + directory);
        return true;
    };

    if (configuredFileName.empty()) {
        LOG_ERROR("FileSource parameter filename is empty");
        return false;
    }

    if (hasWildcardPattern(configuredFileName)) {
        const std::filesystem::path wildcardPath(configuredFileName);
        const std::filesystem::path parent = wildcardPath.parent_path();
        const std::string directory = parent.empty() ? std::string(".") : parent.string();
        return addFromDirectory(directory, wildcardPath.filename().string());
    }

    std::error_code errorCode;
    if (std::filesystem::is_directory(configuredFileName, errorCode)) {
        return addFromDirectory(configuredFileName, "*");
    }

    m_inputFiles.push_back(configuredFileName);
    return true;
}

bool FileSource::loadFile(const std::string& fileName, FrameContext& context)
{
    if (isEncodedImageFile(fileName)) {
        return loadEncodedImage(fileName, context);
    }
    return loadRawImage(fileName, context);
}

bool FileSource::loadEncodedImage(const std::string& fileName, FrameContext& context)
{
    cv::Mat image = cv::imread(fileName, cv::IMREAD_COLOR);
    if (image.empty()) {
        LOG_ERROR("Could not decode image file: " + fileName);
        return false;
    }

    ImageBuffer output;
    output.assign(image.data, image.total() * image.elemSize(), static_cast<uint32_t>(image.cols), static_cast<uint32_t>(image.rows), static_cast<uint32_t>(image.step), PixelFormat::BGR888);
    output.setSequence(++m_sequence);
    context.set("image", std::move(output));
    LOG_INFO("Loaded image file " + fileName);
    return true;
}

bool FileSource::loadRawImage(const std::string& fileName, FrameContext& context)
{
    const std::string configuredFormatText = parameterString("format", std::string("auto"));
    PixelFormat format = parseFormatString(configuredFormatText);
    bool packedMono = isPackedMonoToken(configuredFormatText);
    const PixelFormat parsedFormat = inferRawFormatFromFileName(fileName);
    if (parsedFormat != PixelFormat::Unknown) {
        // If raw metadata in the filename contains a format, it overrides the configured format.
        format = parsedFormat;
        packedMono = fileNameContainsPackedMonoToken(fileName);
    }
    if (!isRawPixelFormat(format)) {
        LOG_ERROR("Raw FileSource requires a supported RAW format: " + fileName);
        return false;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    auto parsedDimensions = parseDimensionsFromFileName(fileName);
    if (parsedDimensions.has_value()) {
        // If dimensions are encoded in filename metadata they override explicit parameters.
        width = parsedDimensions->first;
        height = parsedDimensions->second;
    } else {
        width = static_cast<uint32_t>(parameterInt("width", 0));
        height = static_cast<uint32_t>(parameterInt("height", 0));
    }
    if (width == 0 || height == 0) {
        LOG_ERROR("Raw FileSource requires width and height (or filename token like ...1920x1080...): " + fileName);
        return false;
    }

    std::ifstream file(fileName, std::ios::binary);
    if (!file) {
        LOG_ERROR("Cannot open input file: " + fileName);
        return false;
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (data.empty()) {
        LOG_ERROR("Input file is empty: " + fileName);
        return false;
    }

    uint32_t stride = static_cast<uint32_t>(parameterInt("stride", 0));
    if (stride == 0) {
        if (packedMono && (format == PixelFormat::Mono10 || format == PixelFormat::Mono12)) {
            stride = static_cast<uint32_t>((static_cast<uint64_t>(width) * bitsPerPixel(format) + 7) / 8);
        } else {
            stride = defaultStrideFor(format, width);
        }
    }

    const size_t neededSize = expectedRawSize(format, width, height, stride);
    if (neededSize == 0) {
        LOG_ERROR("Raw FileSource computed invalid target size for: " + fileName);
        return false;
    }

    std::vector<uint8_t> normalizedData(neededSize, 0);
    const size_t copySize = std::min(data.size(), neededSize);
    std::copy_n(data.data(), copySize, normalizedData.data());
    if (copySize != neededSize) {
        LOG_WARNING("Raw FileSource size mismatch for " + fileName + ": file bytes=" + std::to_string(data.size()) + ", expected=" + std::to_string(neededSize) + ". Data was safely clipped/padded.");
    }

    ImageBuffer image;
    image.assign(normalizedData.data(), normalizedData.size(), width, height, stride, format);
    const int64_t configuredBitShift = parameterInt("bitshift", 0);
    image.setBitShift(static_cast<uint8_t>(std::clamp<int64_t>(configuredBitShift, 0, 8)));

    const std::optional<uint64_t> parsedSequence = parseSequenceFromFileName(fileName);
    const uint64_t sequence = parsedSequence.has_value() ? *parsedSequence : (m_sequence + 1);
    m_sequence = std::max(m_sequence, sequence);
    image.setSequence(sequence);

    context.set("image", std::move(image));
    LOG_INFO("Loaded raw image file " + fileName + " as " + pixelFormatToString(format));
    return true;
}

bool FileSource::isEncodedImageFile(const std::string& fileName) const
{
    return endsWithCaseInsensitive(fileName, ".png") || endsWithCaseInsensitive(fileName, ".jpg") || endsWithCaseInsensitive(fileName, ".jpeg");
}

bool FileSource::isSupportedImageFile(const std::string& fileName) const
{
    if (isEncodedImageFile(fileName)) {
        return true;
    }
    if (isRawFileExtension(fileName)) {
        return true;
    }
    if (parseFormatString(parameterString("format", std::string())) != PixelFormat::Unknown) {
        return true;
    }
    return false;
}

std::optional<std::pair<uint32_t, uint32_t>> FileSource::parseDimensionsFromFileName(const std::string& fileName) const
{
    static const std::regex dimensionsRegex(R"((\d{1,6})x(\d{1,6}))", std::regex::icase);
    std::smatch match;
    if (!std::regex_search(fileName, match, dimensionsRegex) || match.size() < 3) {
        return std::nullopt;
    }
    return std::make_pair(static_cast<uint32_t>(std::stoul(match[1].str())), static_cast<uint32_t>(std::stoul(match[2].str())));
}

PixelFormat FileSource::parseFormatString(const std::string& value) const
{
    if (value.empty()) {
        return PixelFormat::Unknown;
    }

    // Packed Bayer tokens are case-sensitive in this codebase (e.g. pRAA/pGAA/pgAA).
    const PixelFormat exact = pixelFormatFromString(value);
    if (exact != PixelFormat::Unknown) {
        return exact;
    }

    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    if (normalized == "AUTO") {
        return PixelFormat::Unknown;
    }

    if (normalized == "RGGB") {
        return PixelFormat::RG8;
    }
    if (normalized == "GBRG") {
        return PixelFormat::GB8;
    }
    if (normalized == "GRBG") {
        return PixelFormat::GR8;
    }
    if (normalized == "BGGR") {
        return PixelFormat::BG8;
    }
    if (normalized == "RG10") {
        return PixelFormat::RG10;
    }
    if (normalized == "GB10") {
        return PixelFormat::GB10;
    }
    if (normalized == "GR10") {
        return PixelFormat::GR10;
    }
    if (normalized == "BG10") {
        return PixelFormat::BG10;
    }
    if (normalized == "RG10P") {
        return PixelFormat::RG10P;
    }
    if (normalized == "GB10P") {
        return PixelFormat::GB10P;
    }
    if (normalized == "GR10P") {
        return PixelFormat::GR10P;
    }
    if (normalized == "BG10P") {
        return PixelFormat::BG10P;
    }
    if (normalized == "RG12") {
        return PixelFormat::RG12;
    }
    if (normalized == "GB12") {
        return PixelFormat::GB12;
    }
    if (normalized == "GR12") {
        return PixelFormat::GR12;
    }
    if (normalized == "BG12") {
        return PixelFormat::BG12;
    }
    if (normalized == "RG12P") {
        return PixelFormat::RG12P;
    }
    if (normalized == "GB12P") {
        return PixelFormat::GB12P;
    }
    if (normalized == "GR12P") {
        return PixelFormat::GR12P;
    }
    if (normalized == "BG12P") {
        return PixelFormat::BG12P;
    }
    if (normalized == "RG14") {
        return PixelFormat::RG14;
    }
    if (normalized == "GB14") {
        return PixelFormat::GB14;
    }
    if (normalized == "GR14") {
        return PixelFormat::GR14;
    }
    if (normalized == "BG14") {
        return PixelFormat::BG14;
    }
    if (normalized == "RG14P") {
        return PixelFormat::RG14P;
    }
    if (normalized == "GB14P") {
        return PixelFormat::GB14P;
    }
    if (normalized == "GR14P") {
        return PixelFormat::GR14P;
    }
    if (normalized == "BG14P") {
        return PixelFormat::BG14P;
    }
    if (normalized == "Y10" || normalized == "Y10P") {
        return PixelFormat::Mono10;
    }
    if (normalized == "Y12" || normalized == "Y12P") {
        return PixelFormat::Mono12;
    }
    if (normalized == "Y14") {
        return PixelFormat::Mono14;
    }

    return pixelFormatFromString(normalized);
}

PixelFormat FileSource::inferRawFormatFromFileName(const std::string& fileName) const
{
    auto containsCaseSensitive = [&fileName](const std::string& token) { return fileName.find(token) != std::string::npos; };
    if (containsCaseSensitive("pRAA")) {
        return PixelFormat::RG10P;
    }
    if (containsCaseSensitive("pRCC")) {
        return PixelFormat::RG12P;
    }
    if (containsCaseSensitive("pREE")) {
        return PixelFormat::RG14P;
    }
    if (containsCaseSensitive("pgAA")) {
        return PixelFormat::GR10P;
    }
    if (containsCaseSensitive("pgCC")) {
        return PixelFormat::GR12P;
    }
    if (containsCaseSensitive("pgEE")) {
        return PixelFormat::GR14P;
    }
    if (containsCaseSensitive("pBAA")) {
        return PixelFormat::BG10P;
    }
    if (containsCaseSensitive("pBCC")) {
        return PixelFormat::BG12P;
    }
    if (containsCaseSensitive("pBEE")) {
        return PixelFormat::BG14P;
    }
    if (containsCaseSensitive("pGAA")) {
        return PixelFormat::GB10P;
    }
    if (containsCaseSensitive("pGCC")) {
        return PixelFormat::GB12P;
    }
    if (containsCaseSensitive("pGEE")) {
        return PixelFormat::GB14P;
    }

    const std::string upperPath = [&fileName]() {
        std::string value = fileName;
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return value;
    }();

    auto contains = [&upperPath](const std::string& token) { return upperPath.find(token) != std::string::npos; };
    if (endsWithCaseInsensitive(fileName, ".yuyv") || contains("YUYV")) {
        return PixelFormat::YUYV;
    }
    if (endsWithCaseInsensitive(fileName, ".nv12") || contains("NV12")) {
        return PixelFormat::NV12;
    }
    if (contains("RG10P")) {
        return PixelFormat::RG10P;
    }
    if (contains("GB10P")) {
        return PixelFormat::GB10P;
    }
    if (contains("GR10P")) {
        return PixelFormat::GR10P;
    }
    if (contains("BG10P")) {
        return PixelFormat::BG10P;
    }
    if (contains("RG12P")) {
        return PixelFormat::RG12P;
    }
    if (contains("GB12P")) {
        return PixelFormat::GB12P;
    }
    if (contains("GR12P")) {
        return PixelFormat::GR12P;
    }
    if (contains("BG12P")) {
        return PixelFormat::BG12P;
    }
    if (contains("RG14P")) {
        return PixelFormat::RG14P;
    }
    if (contains("GB14P")) {
        return PixelFormat::GB14P;
    }
    if (contains("GR14P")) {
        return PixelFormat::GR14P;
    }
    if (contains("BG14P")) {
        return PixelFormat::BG14P;
    }
    if (contains("RG10")) {
        return PixelFormat::RG10;
    }
    if (contains("GB10")) {
        return PixelFormat::GB10;
    }
    if (contains("GR10")) {
        return PixelFormat::GR10;
    }
    if (contains("BG10")) {
        return PixelFormat::BG10;
    }
    if (contains("RG12")) {
        return PixelFormat::RG12;
    }
    if (contains("GB12")) {
        return PixelFormat::GB12;
    }
    if (contains("GR12")) {
        return PixelFormat::GR12;
    }
    if (contains("BG12")) {
        return PixelFormat::BG12;
    }
    if (contains("RG14")) {
        return PixelFormat::RG14;
    }
    if (contains("GB14")) {
        return PixelFormat::GB14;
    }
    if (contains("GR14")) {
        return PixelFormat::GR14;
    }
    if (contains("BG14")) {
        return PixelFormat::BG14;
    }
    if (contains("RGGB")) {
        return PixelFormat::RG8;
    }
    if (contains("GBRG")) {
        return PixelFormat::GB8;
    }
    if (contains("GRBG")) {
        return PixelFormat::GR8;
    }
    if (contains("BGGR")) {
        return PixelFormat::BG8;
    }
    if (contains("Y10P")) {
        return PixelFormat::Mono10;
    }
    if (contains("Y12P")) {
        return PixelFormat::Mono12;
    }
    if (contains("Y10")) {
        return PixelFormat::Mono10;
    }
    if (contains("Y12")) {
        return PixelFormat::Mono12;
    }
    if (contains("Y14")) {
        return PixelFormat::Mono14;
    }
    if (contains("GREY")) {
        return PixelFormat::Mono8;
    }
    return PixelFormat::Unknown;
}
