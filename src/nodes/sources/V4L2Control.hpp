// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "parameters/Parameter.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

/**
 * @brief Descriptor for a single V4L2 camera control mapped to a @ref Node parameter.
 *
 * V4L2Control stores the metadata for one V4L2 control discovered on a
 * video device: its Linux kernel control ID, name, value range, type
 * and the corresponding @ref ParameterInfo name used in CamFlow.
 *
 * Instances are created by @ref V4L2ControlAccess::enumerate and stored
 * in @ref V4L2Source for schema generation and runtime control updates.
 */
struct V4L2Control
{
    std::string parameterName;        ///< CamFlow parameter name (e.g. `"exposure"`).
    std::string controlName;          ///< V4L2 control name string from the kernel.
    uint32_t id = 0;                  ///< V4L2 control identifier (V4L2_CID_*).
    uint32_t type = 0;                ///< V4L2 control type (V4L2_CTRL_TYPE_*).
    int64_t minimum = 0;              ///< Minimum allowed value.
    int64_t maximum = 0;              ///< Maximum allowed value.
    int64_t step = 0;                 ///< Granularity step between values.
    int64_t defaultValue = 0;         ///< Driver default value.
    int fd = -1;                      ///< Open file descriptor for the device that owns this control.
    bool writable = true;             ///< @c true if the control is not read-only.
    std::vector<std::string> options; ///< Valid option strings for menu controls.
    std::string sourceDevice;         ///< Human-readable V4L2 source device path.
};

/**
 * @brief Utility class for enumerating, reading and writing V4L2 camera controls.
 *
 * V4L2ControlAccess provides static methods to interact with V4L2 device controls
 * at the system level. It is used exclusively by @ref V4L2Source to:
 * - Discover all available controls on a device at init time.
 * - Read and write individual control values during parameter updates.
 * - Convert V4L2 control metadata to CamFlow @ref ParameterInfo descriptors.
 *
 * All methods perform direct ioctl calls and do not retain state.
 */
class V4L2ControlAccess
{
public:
    /**
     * @brief Enumerates all controls on the given device (and optional sub-device).
     *
     * Queries the kernel for all available V4L2 controls on @p deviceFd and
     * @p subDeviceFd (if >= 0) and returns a list of @ref V4L2Control descriptors.
     *
     * @param deviceFd     Open file descriptor of the V4L2 video device.
     * @param subDeviceFd  Open file descriptor of the V4L2 sub-device, or @c -1 if none.
     * @return List of discovered controls.
     */
    static std::vector<V4L2Control> enumerate(int deviceFd, const std::string& deviceName, int subDeviceFd, const std::string& subDeviceName);

    /**
     * @brief Reads the current value of a V4L2 control.
     * @param control Control descriptor (must have a valid fd and id).
     * @param value   Output parameter; receives the current control value.
     * @return @c true on success; @c false if the ioctl failed.
     */
    static bool read(const V4L2Control& control, int64_t& value);

    /**
     * @brief Writes a new value to a V4L2 control.
     * @param control Control descriptor (must have a valid fd and id).
     * @param value   New value to write, clamped to [minimum, maximum].
     * @return @c true on success; @c false if the ioctl failed or the control is read-only.
     */
    static bool write(const V4L2Control& control, int64_t value);

    /**
     * @brief Converts a @ref V4L2Control to a @ref ParameterInfo descriptor.
     *
     * Maps V4L2 control type to the appropriate @ref ParameterType and
     * sets the name, description, range and default value.
     *
     * @param control Source control descriptor.
     * @return Corresponding @ref ParameterInfo.
     */
    static ParameterInfo toParameterInfo(const V4L2Control& control);

    /**
     * @brief Converts a raw V4L2 control name string to a CamFlow parameter name.
     *
     * Converts the kernel control name (e.g. `"Exposure (Absolute)"`) to a
     * lowercase, underscore-separated parameter name (e.g. `"exposure_absolute"`).
     *
     * @param name Raw kernel control name.
     * @return Normalised CamFlow parameter name.
     */
    static std::string parameterNameFromControlName(const std::string& name);

private:
    /**
     * @brief Enumerates controls on a single file descriptor and appends to @p controls.
     * @param fd       Open device file descriptor.
     * @param controls Output list to append discovered controls to.
     * @param names    Map used to detect and resolve duplicate control names.
     */
    static void enumerateFd(int fd, std::vector<V4L2Control>& controls, std::map<std::string, int>& names);
};
