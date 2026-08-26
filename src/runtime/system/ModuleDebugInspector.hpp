// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include <string>

class ModuleDebugInspector
{
public:
    static std::string listJson();
    static bool setDebugLevel(const std::string& module, const std::string& value, std::string& errorMessage);
};
