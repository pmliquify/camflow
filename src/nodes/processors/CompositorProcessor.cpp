// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "CompositorProcessor.hpp"

#include "core/Logger.hpp"
#include "image/ImageBuffer.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace
{

struct LayerConfig
{
    std::string scope;
    const ImageBuffer* source = nullptr;
    ImageBuffer converted;
    int64_t xPos = 0;
    int64_t yPos = 0;
    int64_t zOrder = 0;
    size_t order = 0;
};

cv::Size sourceSize(const LayerConfig& layer)
{
    return {static_cast<int>(layer.source->width()), static_cast<int>(layer.source->height())};
}

} // namespace

std::string CompositorProcessor::typeName() const
{
    return "compositor";
}

std::string CompositorProcessor::description() const
{
    return "Composites bound input images using xpos, ypos and zorder lists.";
}

NodeSchema CompositorProcessor::schema() const
{
    NodeSchema schema;
    schema.parameters = {{"xpos", ParameterType::String, "Comma-separated x positions per input image, e.g. 0,100", std::string("0"), std::string(), std::string(), {}, true},
                         {"ypos", ParameterType::String, "Comma-separated y positions per input image, e.g. 0,0", std::string("0"), std::string(), std::string(), {}, true},
                         {"zorder", ParameterType::String, "Comma-separated z-order values per input image, e.g. 0,1", std::string("0"), std::string(), std::string(), {}, true}};
    schema.inputs = {NodeInputInfo{"image", "image", "Input image binding(s) in the form image=<nodeId>.<output>", true}};
    schema.outputs = {NodeOutputInfo{"image", "image", "Composited image"}};
    return schema;
}

bool CompositorProcessor::process(FrameContext& context)
{
    IImageConverter* imageConverter = converter();
    if (imageConverter == nullptr) {
        return false;
    }

    const auto imageBindings = resolveInputBindings("image", context);
    if (imageBindings.empty()) {
        LOG_ERROR("CompositorProcessor requires at least one image binding, e.g. image=cam0.image,cam1.image");
        return false;
    }

    auto parseIntList = [](const std::string& text) {
        std::vector<int64_t> values;
        std::string token;
        std::istringstream stream(text);
        while (std::getline(stream, token, ',')) {
            size_t begin = 0;
            while (begin < token.size() && std::isspace(static_cast<unsigned char>(token[begin]))) {
                ++begin;
            }
            size_t end = token.size();
            while (end > begin && std::isspace(static_cast<unsigned char>(token[end - 1]))) {
                --end;
            }
            if (end <= begin) {
                continue;
            }
            try {
                values.push_back(std::stoll(token.substr(begin, end - begin)));
            } catch (...) {
                values.push_back(0);
            }
        }
        if (values.empty()) {
            values.push_back(0);
        }
        return values;
    };

    const std::vector<int64_t> xPositions = parseIntList(parameterString("xpos", "0"));
    const std::vector<int64_t> yPositions = parseIntList(parameterString("ypos", "0"));
    const std::vector<int64_t> zOrders = parseIntList(parameterString("zorder", "0"));

    std::vector<LayerConfig> layers;
    layers.reserve(imageBindings.size());

    for (size_t i = 0; i < imageBindings.size(); ++i) {
        const std::string& scope = imageBindings[i].first;
        const std::string& key = imageBindings[i].second;
        const ImageBuffer* image = scope.empty() ? context.get<ImageBuffer>(key) : context.get<ImageBuffer>(scope, key);
        if (image == nullptr) {
            return false;
        }
        if (image->width() == 0 || image->height() == 0) {
            const std::string imageKey = scope.empty() ? key : (scope + "." + key);
            LOG_ERROR("CompositorProcessor requires non-empty image input '" + imageKey + "'");
            return false;
        }

        LayerConfig layer;
        layer.scope = scope.empty() ? key : (scope + "." + key);
        layer.source = image;
        layer.order = i;

        layer.xPos = xPositions[std::min(i, xPositions.size() - 1)];
        layer.yPos = yPositions[std::min(i, yPositions.size() - 1)];
        layer.zOrder = zOrders[std::min(i, zOrders.size() - 1)];

        if (!imageConverter->convert(*image, layer.converted, PixelFormat::BGR888)) {
            const std::string imageKey = scope.empty() ? key : (scope + "." + key);
            LOG_ERROR("CompositorProcessor failed to convert input '" + imageKey + "' to BGR888");
            return false;
        }

        layers.push_back(std::move(layer));
    }

    std::stable_sort(layers.begin(), layers.end(), [](const LayerConfig& lhs, const LayerConfig& rhs) {
        if (lhs.zOrder != rhs.zOrder) {
            return lhs.zOrder < rhs.zOrder;
        }
        return lhs.order < rhs.order;
    });

    int64_t minX = 0;
    int64_t minY = 0;
    int64_t maxX = 0;
    int64_t maxY = 0;
    for (const auto& layer : layers) {
        const cv::Size size = sourceSize(layer);
        minX = std::min(minX, layer.xPos);
        minY = std::min(minY, layer.yPos);
        maxX = std::max(maxX, layer.xPos + size.width);
        maxY = std::max(maxY, layer.yPos + size.height);
    }

    const cv::Size canvasSize(static_cast<int>(std::max<int64_t>(1, maxX - minX)), static_cast<int>(std::max<int64_t>(1, maxY - minY)));
    cv::Mat canvas(canvasSize, CV_8UC3, cv::Scalar(0, 0, 0));
    const cv::Rect canvasBounds(0, 0, canvasSize.width, canvasSize.height);

    for (const auto& layer : layers) {
        const cv::Size size = sourceSize(layer);
        const cv::Rect destinationRect(static_cast<int>(layer.xPos - minX), static_cast<int>(layer.yPos - minY), size.width, size.height);
        const cv::Rect clippedRect = destinationRect & canvasBounds;
        if (clippedRect.empty()) {
            continue;
        }

        const cv::Rect sourceRect(clippedRect.x - destinationRect.x, clippedRect.y - destinationRect.y, clippedRect.width, clippedRect.height);
        cv::Mat sourceMat(size.height, size.width, CV_8UC3, const_cast<uint8_t*>(layer.converted.data()), layer.converted.stride());
        sourceMat(sourceRect).copyTo(canvas(clippedRect));
    }

    ImageBuffer output;
    output.allocate(static_cast<uint32_t>(canvas.cols), static_cast<uint32_t>(canvas.rows), static_cast<uint32_t>(canvas.cols * 3), PixelFormat::BGR888);
    cv::Mat outputMat(static_cast<int>(output.height()), static_cast<int>(output.width()), CV_8UC3, output.data(), output.stride());
    canvas.copyTo(outputMat);

    uint64_t sequence = 0;
    uint64_t timestampNs = 0;
    for (const auto& layer : layers) {
        sequence = std::max(sequence, layer.source->sequence());
        timestampNs = std::max(timestampNs, layer.source->timestampNs());
    }

    output.setSequence(sequence);
    output.setTimestampNs(timestampNs);
    context.set("image", std::move(output));
    return true;
}