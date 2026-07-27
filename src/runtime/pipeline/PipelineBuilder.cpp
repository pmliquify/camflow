// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "PipelineBuilder.hpp"

#include "core/Logger.hpp"
#include "pipeline/Pipeline.hpp"
#include "pipeline/PipelineGraph.hpp"

PipelineBuilder::PipelineBuilder(const NodeFactory& factory, IImageConverter* converter) :
    m_factory(factory),
    m_converter(converter)
{
}

std::unique_ptr<IPipeline> PipelineBuilder::build(const GraphConfig& config, bool useSimplePipeline) const
{
    std::unique_ptr<IPipeline> pipeline;
    if (useSimplePipeline) {
        pipeline = std::make_unique<Pipeline>();
    } else {
        pipeline = std::make_unique<PipelineGraph>();
    }

    for (const auto& nodeConfig : config.nodes()) {
        NodePtr node = m_factory.create(nodeConfig.type);
        if (!node) {
            LOG_ERROR("Unknown node type: " + nodeConfig.type);
            return nullptr;
        }

        node->setId(nodeConfig.id);
        node->setImageConverter(m_converter);
        node->configure(nodeConfig.parameters);
        pipeline->addNode(std::move(node));
    }
    for (const auto& edge : config.edges()) {
        pipeline->addEdge(edge);
    }
    return pipeline;
}
