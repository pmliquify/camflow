// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "image/IImageConverter.hpp"
#include "parameters/ParameterSet.hpp"
#include "network/FrameContext.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class Pipeline;

/**
 * @brief Abstract base class for all pipeline nodes.
 *
 * Node defines the lifecycle and processing contract that every pipeline element
 * must implement. A node can be a source (camera, file, network input), a
 * processor (debayer, compositor, color correction), a sink (GUI, file writer,
 * TCP output), or a diagnostic sink such as @ref LogSink.
 *
 * The class also provides the common storage for configured parameters, node ID
 * and shared infrastructure such as the image converter. Derived nodes focus on
 * their actual processing logic and use the runtime context to read the current
 * frame state.
 *
 * ### Lifecycle
 * | Phase      | Method            | Called by         |
 * |------------|-------------------|-------------------|
 * | Setup      | setId             | PipelineBuilder   |
 * | Setup      | setImageConverter  | PipelineBuilder   |
 * | Configure  | configure         | PipelineBuilder   |
 * | Init       | init             | IPipeline::init   |
 * | Start      | start            | IPipeline::start  |
 * | Processing | process          | IPipeline::run    |
 * | Stop       | stop             | IPipeline::stop   |
 * | Shutdown   | shutdown         | IPipeline::shutdown |
 *
 * ### Context contract
 * Nodes declare their context contract by overriding @ref schema. The schema is
 * split into three sections: configurable `parameters`, typed `inputs`, and
 * typed `outputs`.
 *
 * Runtime updates are handled by @ref setParameter in the base class. Derived
 * nodes can react to actual value changes through @ref onParameterChanged.
 *
 * ### Image conversion
 * The image converter is injected once by the builder and is exposed to derived
 * classes through @ref converter(). Nodes that need to convert image data should
 * access the converter via that helper so the dependency is explicit and missing
 * infrastructure can be reported with the requesting node id.
 *
 * @see IPipeline
 * @see FrameContext
 * @see NodeFactory
 */
class Node
{
public:
    virtual ~Node();

    /**
     * @brief Sets the unique identifier of this node instance.
     *
     * The identifier is used throughout the pipeline for edge wiring, profiling
     * reports and REST API addressing. It must be unique within a @ref GraphConfig.
     *
     * @param id Identifier string (e.g. `"cam0"`, `"sink0"`).
     */
    void setId(const std::string& id);

    /**
     * @brief Returns the unique identifier of this node.
     * @return The identifier set via @ref setId.
     */
    const std::string& id() const;

    /**
     * @brief Injects the application-wide image converter.
     *
     * Called by @ref PipelineBuilder after construction. Nodes that need to
     * convert image formats use this converter directly. The pointer is
     * non-owning; the converter outlives all nodes.
     *
     * @param converter Pointer to the converter; may be @c nullptr.
     */
    void setImageConverter(IImageConverter* converter);

    /**
     * @brief Returns the unique type name string registered in @ref NodeFactory.
     *
     * The type name is used as the factory lookup key and as the node name in
     * pipeline DSL expressions (e.g. `"v4l2src"`, `"tcpsink"`).
     *
     * @return Lowercase type name string.
     */
    virtual std::string typeName() const = 0;

    /**
     * @brief Returns a human-readable description of this node type.
     *
     * The description appears in generated help output, documentation and the
     * REST API. The default implementation returns an empty string so that node
     * types can opt in to richer descriptions only when needed.
     *
     * @return Description string.
     */
    virtual std::string description() const;

    /**
     * @brief Returns the parameter schema for this node type.
     *
     * The schema declares all configurable parameters with their names, types,
     * descriptions, default values, valid ranges and writability flags. It is the
     * source of truth for generated help, REST metadata and CLI validation.
     *
     * The default implementation returns an empty schema.
     *
     * @return Full @ref NodeSchema descriptor.
     */
    virtual NodeSchema schema() const;

    /**
     * @brief Applies the given parameter set to the node.
     *
     * Called once by @ref PipelineBuilder after construction. Implementations
     * should parse static setup options and cache defaults where needed. The base
     * implementation stores the full set so the scheduler can publish it into
     * @ref FrameContext on every frame.
     *
     * @param parameters The parameter set from the @ref NodeConfig.
     * @return @c true if all required parameters are valid; @c false otherwise.
     */
    virtual bool configure(const ParameterSet& parameters);

    /**
     * @brief Updates a single parameter at runtime.
     *
     * Called by @ref RuntimeController when a parameter change arrives from the
     * REST API or another runtime control path. The base implementation enforces
     * schema metadata (`runtimeWritable`), stores the new
     * value and calls @ref onParameterChanged when the value actually changed.
     *
     * @param name  Parameter name as declared in @ref schema.
     * @param value New parameter value.
     * @param allowLocked Allows non-runtime-writable parameters while the runtime is stopped.
     * @param errorMessage Optional output for a rejection reason.
     * @return @c true if the parameter was accepted; @c false if unknown or invalid.
     */
    virtual bool setParameter(const std::string& name, const ParameterValue& value, bool allowLocked = false, std::string* errorMessage = nullptr);
    std::vector<std::pair<std::string, std::string>> inputBindings(const std::string& name) const;

    /**
     * @brief Resolves active bindings for one input against current context.
     *
     * Returns explicit bindings when configured, otherwise falls back to
     * available read scopes for the same input key.
     *
     * @param name Input name.
     * @param context Current frame context.
     * @return Resolved `<scope, key>` references.
     */
    std::vector<std::pair<std::string, std::string>> resolveInputBindings(const std::string& name, const FrameContext& context) const;

    /**
     * @brief Performs one-time initialisation before the processing loop starts.
     *
     * Called once by @ref IPipeline::init after all nodes have been configured.
     * Implementations should open hardware resources, allocate buffers, or
     * establish connections here. The default implementation returns @c true
     * unconditionally.
     *
     * @return @c true if initialisation succeeded; @c false to abort the pipeline.
     */
    virtual bool init();

    /**
     * @brief Performs node-specific startup after initialisation.
     *
     * Called by @ref IPipeline::start after all nodes have been initialised.
     * The default implementation returns @c true.
     */
    virtual bool start();

    /**
     * @brief Processes one frame of data.
     *
     * This is the main execution entry point for the node. The pipeline invokes
     * it once per frame after publishing the current context state into the
     * node's configured write scope. Implementations typically read their inputs
     * from @p context, transform or combine them, and write their outputs back to
     * the same context.
     *
     * Returning @c false signals end-of-stream or a fatal error and causes the
     * pipeline to stop after the current frame.
     *
     * @param context  Shared per-frame data carrier; read inputs and write outputs here.
     * @return @c true to continue processing; @c false to stop the pipeline.
     */
    virtual bool process(FrameContext& context) = 0;

    /**
     * @brief Returns a snapshot of the active parameters.
     *
     * Derived nodes may override this when additional locking is required.
     *
     * @return Copy of the currently active parameter set.
     */
    virtual ParameterSet currentParameters() const;

    /**
     * @brief Performs orderly shutdown after the processing loop ends.
     *
     * Called once by @ref IPipeline::shutdown in reverse registration order.
     * Implementations should stop streaming, close file handles and release
     * hardware resources. The default implementation is a no-op.
     */
    virtual void shutdown();

    /**
     * @brief Performs node-specific teardown before shutdown.
     *
     * Called by @ref IPipeline::stop before @ref shutdown. The default implementation is a no-op.
     */
    virtual void stop();

protected:
    /**
     * @brief Returns the injected image converter for this node.
     *
     * If no converter was injected, an error is logged that includes the node id
     * of the caller and @c nullptr is returned. Derived classes should use this
     * helper instead of accessing the member directly so the dependency remains
     * explicit.
     *
     * @return Non-owning pointer to the global converter, or @c nullptr if the
     *         converter is unavailable.
     */
    IImageConverter* converter() const;

    /**
     * @brief Returns the current parameter value for @p name, if available.
     * @param name Parameter name.
     * @return Pointer to value or @c nullptr when not configured.
     */
    const ParameterValue* parameter(const std::string& name) const;

    /**
     * @brief Returns @p name as bool with conversion and fallback.
     */
    bool parameterBool(const std::string& name, bool fallback) const;

    /**
     * @brief Returns @p name as int64 with conversion and fallback.
     */
    int64_t parameterInt(const std::string& name, int64_t fallback) const;

    /**
     * @brief Returns @p name as double with conversion and fallback.
     */
    double parameterDouble(const std::string& name, double fallback) const;

    /**
     * @brief Returns @p name as string with conversion and fallback.
     */
    std::string parameterString(const std::string& name, const std::string& fallback) const;

    /**
     * @brief Returns the complete configured parameter set.
     *
     * This is primarily used by source nodes that need to iterate dynamic
     * hardware controls discovered at runtime.
     *
     * @return Read-only configured values.
     */
    const ParameterSet& configuredParameters() const;

    /**
     * @brief Called by @ref setParameter after a value actually changed.
     *
     * Derived classes can react to parameter updates without overriding the
     * generic parameter storage behavior in @ref Node.
     *
     * @param name Parameter name.
     * @param value New parameter value.
     * @param previousValue Previous value pointer, or @c nullptr when no previous value existed.
     * @param errorMessage Output for a node-specific rejection reason.
     * @return @c true if the update was applied; @c false to restore the previous value.
     */
    virtual bool onParameterChanged(const std::string& name, const ParameterValue& value, const ParameterValue* previousValue, std::string& errorMessage);

    /**
     * @brief Overwrites the stored value for @p name without invoking @ref onParameterChanged.
     *
     * Used by sources whose device-derived defaults (e.g. hardware-reported format) can only
     * be discovered after @ref configure has already populated @ref m_parameters with a
     * placeholder schema default; keeps the stored value in sync with reality so a later
     * @ref setParameter call with that same real value is not mistaken for a no-op.
     *
     * @param name Parameter name.
     * @param value Value to store.
     */
    void syncParameterValue(const std::string& name, const ParameterValue& value);

private:
    friend class Pipeline;

    /// Internal parameter access used by the scheduler for runtime publication.
    const ParameterSet& runtimeParameters() const;

    /// Internal schema lookup used by the scheduler and runtime updates.
    const ParameterInfo* parameterInfo(const std::string& name) const;

    ParameterSet m_parameters;                                                               ///< Currently active parameter values.
    std::map<std::string, ParameterInfo> m_schemaByName;                                     ///< Cached parameter metadata keyed by name.
    std::map<std::string, NodeInputInfo> m_inputSchemaByName;                                ///< Cached input metadata keyed by name.
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> m_inputBindings; ///< Explicit input bindings.
    std::string m_id;                                                                        ///< Unique node identifier within the pipeline.
    IImageConverter* m_converter = nullptr;                                                  ///< Injected converter (non-owning).
};