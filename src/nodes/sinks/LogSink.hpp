// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "pipeline/Node.hpp"

#include <cstdint>

/** @brief Logs timing and format information for each received image. */
class LogSink : public Node
{
public:
    std::string typeName() const override;
    std::string description() const override;
    NodeSchema schema() const override;
    bool process(FrameContext& context) override;

private:
    bool m_hasPreviousTimestamp = false;
    uint64_t m_previousTimestampNs = 0;
};