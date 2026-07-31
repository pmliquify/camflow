// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "image/IImageConverter.hpp"

/**
 * @brief Image format converter backed by the OpenCV library.
 *
 * ImageConverter implements @ref IImageConverter using OpenCV's
 * `cv::cvtColor` and related functions to perform pixel format conversions.
 * It supports conversions from various Bayer and raw formats to display-ready
 * formats such as @ref PixelFormat::RGB888 and @ref PixelFormat::Mono8.
 *
 * ## Conversion strategy (implementation overview)
 *
 * The converter uses a deterministic pipeline depending on source and target
 * format. The high-level flow is:
 *
 * 1. Fast-path passthrough:
 *    - If source and destination format are equal, bytes are copied as-is.
 * 2. Target dispatch:
 *    - The request is dispatched to one of the dedicated paths:
 *      @ref convertToMono8, @ref convertToBgr888, @ref convertToYuyv,
 *      @ref convertToNv12, or an RGB path via BGR.
 * 3. RAW/Bayer normalization:
 *    - RAW and Bayer inputs are normalized to an intermediate 16-bit mono
 *      image before further conversion.
 *    - Packed formats (for example 10/12/14-bit packed Bayer) are unpacked
 *      bitwise to 16-bit samples.
 *    - Unpacked >8-bit formats are read as 16-bit samples.
 *    - If ImageBuffer::bitShift is set, values are divided by 2^bitShift
 *      using OpenCV arithmetic before debayer/8-bit reduction.
 * 4. Precision reduction:
 *    - For Mono8 output (and Bayer pre-processing), the intermediate 16-bit
 *      data is right-shifted to 8-bit according to source bit depth.
 * 5. Color conversion:
 *    - Bayer to BGR uses OpenCV Bayer codes selected from the Bayer pattern.
 *    - RGB/BGR/GRAY/YUYV/NV12 conversions use OpenCV color conversion APIs.
 * 6. YUV packing:
 *    - BGR to YUYV and BGR to NV12 are generated explicitly using BT.601
 *      luma/chroma equations and chroma subsampling.
 * 7. Metadata propagation:
 *    - Sequence and timestamp are copied to all generated output buffers.
 *
 * ## Conversion flow (ASCII)
 *
 * RAW and Bayer path:
 *
 *   RAW/Bayer input
 *   (Mono10/12/14/16,
 *    RGGB/GBRG/GRBG/BGGR,
 *    RG10/12/14, RG10P/12P/14P, ...)
 *            |
 *            v
 *      [makeMono16]
 *   unpack packed bits if needed
 *            |
 *            v
 *   apply bitShift: value / 2^bitShift
 *            |
 *            +------------------------------+
 *            |                              |
 *            v                              v
 *   [to Mono8 by bit-depth shift]     [Bayer demosaic]
 *            |                         (OpenCV Bayer code)
 *            |                              |
 *            v                              v
 *        Mono8 output                    BGR888
 *                                           |
 *                          +----------------+----------------+
 *                          |                                 |
 *                          v                                 v
 *                       RGB888                          YUYV / NV12
 *
 * Special color format path:
 *
 *   RGB888 ---------> BGR888 ---------> YUYV
 *      ^                 |                |
 *      |                 v                v
 *      +-------------  Mono8 <-------- NV12
 *
 * Notes:
 * - RGB output is always routed through BGR.
 * - YUYV/NV12 input is first converted to BGR, then to target format.
 * - YUYV requires even width; NV12 requires even width and height.
 *
 * ## Format routing notes
 *
 * - RGB output is produced via BGR (BGR first, then channel swap).
 * - YUYV/NV12 inputs are first expanded to BGR for conversions to RGB/Mono.
 * - RAWx and high-bit mono paths are routed through Mono8/BGR helper paths.
 * - YUYV requires even width, NV12 requires even width and height.
 *
 * The application owns and injects this converter through
 * @ref Application::registerConverters.
 *
 * ### Cost model
 * Returns a cost of @c 1 for supported conversions (low priority allows other
 * converters with cost @c 0 to take precedence). Returns @c -1 for unsupported pairs.
 *
 * @see IImageConverter
 * @see Application::registerConverters
 */
class ImageConverter : public IImageConverter
{
public:
    /**
     * @brief Returns the conversion cost for the given format pair.
     * @param sourceFormat       Pixel format of the input image.
     * @param destinationFormat  Desired output pixel format.
     * @return @c 1 for supported pairs; @c -1 for unsupported pairs.
     */
    int cost(PixelFormat sourceFormat, PixelFormat destinationFormat) const override;

    /**
     * @brief Converts @p source to @p destinationFormat using OpenCV.
     * @param source             Read-only input image buffer.
     * @param destination        Output image buffer; filled with converted data.
     * @param destinationFormat  Target pixel format.
     * @return @c true on success; @c false if the conversion failed.
     */
    bool convert(const ImageBuffer& source, ImageBuffer& destination, PixelFormat destinationFormat) override;

private:
    /**
     * @brief Converts the source image to packed 4:2:2 YUYV.
     * @param source      Input image buffer.
     * @param destination Output image buffer.
     * @return @c true on success.
     */
    bool convertToYuyv(const ImageBuffer& source, ImageBuffer& destination);

    /**
     * @brief Converts the source image to semi-planar 4:2:0 NV12.
     * @param source      Input image buffer.
     * @param destination Output image buffer.
     * @return @c true on success.
     */
    bool convertToNv12(const ImageBuffer& source, ImageBuffer& destination);

    /**
     * @brief Converts the source image to 8-bit greyscale.
     * @param source      Input image buffer.
     * @param destination Output image buffer.
     * @return @c true on success.
     */
    bool convertToMono8(const ImageBuffer& source, ImageBuffer& destination);

    /**
     * @brief Converts the source image to 24-bit BGR (OpenCV native format).
     * @param source      Input image buffer.
     * @param destination Output image buffer.
     * @return @c true on success.
     */
    bool convertToBgr888(const ImageBuffer& source, ImageBuffer& destination);
};
