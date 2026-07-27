// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "image/IImageConverter.hpp"
#include "pipeline/NodeFactory.hpp"
#include "pipeline/GraphConfig.hpp"
#include "pipeline/IPipeline.hpp"

/**
 * @brief Factory that constructs a fully wired @ref IPipeline from a @ref GraphConfig.
 *
 * PipelineBuilder is responsible for translating the declarative @ref GraphConfig
 * description (a set of node configurations and directed edges) into a runnable
 * @ref IPipeline instance. It encapsulates:
 *
 * - Selection between @ref PipelineGraph (graph-aware, multi-threaded) and
 *   @ref Pipeline (sequential, single-threaded) via the @p useSimplePipeline flag.
 * - Node creation through the injected @ref NodeFactory.
 * - Node configuration: each node receives its identifier, the shared
 *   @ref IImageConverter pointer and its parameter set from the @ref NodeConfig.
 * - Edge registration: all edges from the @ref GraphConfig are forwarded to the pipeline.
 *
 * The builder does not own the factory or the converter registry; they are injected
 * at construction time and must outlive the builder.
 *
 * ### Example
 * @code
 * PipelineBuilder builder(factory, &converters);
 * auto pipeline = builder.build(graphConfig, useSimplePipeline);
 * if (!pipeline) { // node type not found or creation failed
 *     return 1;
 * }
 * pipeline->setProfilingEnabled(true);
 * pipeline->init();
 * pipeline->run(0);
 * pipeline->shutdown();
 * @endcode
 *
 * @see IPipeline
 * @see PipelineGraph
 * @see Pipeline
 * @see GraphConfig
 * @see NodeFactory
 */
class PipelineBuilder
{
public:
    /**
     * @brief Constructs the builder with the required dependencies.
     *
     * @param factory    Reference to the application-wide @ref NodeFactory used to
     *                   instantiate nodes by their type name string.
     * @param converter Pointer to the shared @ref IImageConverter that will be
     *                  injected into every constructed node. May be @c nullptr if
     *                  no image conversion is needed.
     */
    PipelineBuilder(const NodeFactory& factory, IImageConverter* converter);

    /**
     * @brief Builds and wires a complete pipeline from the given graph configuration.
     *
     * Iterates over all @ref NodeConfig entries in @p config:
     * -# Creates each node via @ref NodeFactory::create.
     * -# Sets the node identifier, injects the shared converter, and applies parameters.
     * -# Adds the node to the pipeline.
     *
     * Then registers all edges from @p config with the pipeline.
     *
     * @param config             The graph configuration describing all nodes and edges.
     * @param useSimplePipeline  If @c true, constructs a @ref Pipeline (linear,
     *                           single-threaded). If @c false, constructs a @ref PipelineGraph
     *                           (graph-aware, multi-threaded).
     * @return Owning pointer to the fully wired pipeline, or @c nullptr if any node
     *         type in @p config could not be resolved by the factory.
     */
    std::unique_ptr<IPipeline> build(const GraphConfig& config, bool useSimplePipeline) const;

private:
    const NodeFactory& m_factory; ///< Node creation factory (non-owning reference).
    IImageConverter* m_converter; ///< Shared converter injected into all nodes.
};
