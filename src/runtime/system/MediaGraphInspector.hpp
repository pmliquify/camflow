// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include <string>

class MediaGraphInspector
{
public:
    static std::string devicesJson();
    static bool graphJson(const std::string& device, std::string& json, std::string& errorMessage);
};
