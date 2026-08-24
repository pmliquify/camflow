// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include <string>

class DeviceTreeInspector
{
public:
    static bool treeJson(std::string& json, std::string& errorMessage);
};
