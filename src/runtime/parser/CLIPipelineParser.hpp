// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "pipeline/GraphConfig.hpp"

#include <set>
#include <string>
#include <vector>

/**
 * @brief Parser for the CamFlow CLI pipeline DSL that produces a @ref GraphConfig.
 *
 * CLIPipelineParser translates a compact, human-writable pipeline expression into a
 * fully wired @ref GraphConfig ready for @ref PipelineBuilder. The DSL is designed
 * for fast command-line use and supports linear chains, branches, merges and sinks.
 *
 * ### DSL syntax summary
 * | Construct            | Syntax example                                      |
 * |----------------------|-----------------------------------------------------|
 * | Linear chain         | `v4l2src->tcpsink`                                  |
 * | With parameters      | `v4l2src(device=/dev/video0)->tcpsink`              |
 * | Input binding        | `compositor(image=cam0.image,cam1.image,xpos=0,100)`|
 * | Branch (parallel)    | `src -> (proc1->sink1, proc2->sink2)`               |
 * | Merge                | `(src1,src2) -> merge -> sink`                      |
 *
 * For a complete formal specification see `docs/cli_pipeline_syntax.md`.
 *
 * ### Parsing strategy
 * The parser operates in three recursive stages:
 * -# **Top-level splitting** – commas at depth-0 split the expression into independent
 *    parallel chains.
 * -# **Chain tokenisation** – each chain is split into node segments at `->` links.
 * -# **Segment parsing** – segments that are wrapped in parentheses are recursively
 *    parsed as sub-graphs (branches or merges).
 *
 * @see GraphConfig
 * @see JsonPipelineParser
 */
class CLIPipelineParser
{
public:
    /**
     * @brief Parses a CLI pipeline expression and populates @p config.
     *
     * @param text         The pipeline DSL string (e.g. `"v4l2src->tcpsink"`).
     * @param config       Output parameter; receives all nodes and edges on success.
     * @param errorMessage Output parameter; descriptive error message on failure.
     * @return @c true on success; @c false if the expression is malformed.
     */
    bool parse(const std::string& text, GraphConfig& config, std::string& errorMessage) const;

private:
    /// Tracks auto-generated indices and used identifiers across the entire parse.
    struct ParseContext
    {
        size_t nodeIndex = 0;          ///< Counter for auto-generated node identifiers.
        std::set<std::string> usedIds; ///< Already used identifiers (for uniqueness).
    };

    /**
     * @brief Parses a top-level expression, splitting on commas to handle parallel chains.
     * @return @c true on success.
     */
    bool parseTopLevel(const std::string& text, GraphConfig& config, ParseContext& context, std::string& errorMessage) const;

    /**
     * @brief Parses a single chain of segments connected by link tokens.
     *
     * @param text            Chain expression text to parse.
     * @param inputFrontier   Node ids whose output feeds into the start of this chain.
     * @param outputFrontier  Receives the ids of the last nodes produced by this chain.
     * @param config          Graph configuration that receives parsed nodes and edges.
     * @param context         Parse context with counters and used ids.
     * @param errorMessage    Output parameter for a descriptive parse error.
     * @return @c true on success.
     */
    bool parseChain(const std::string& text, const std::vector<std::string>& inputFrontier, std::vector<std::string>& outputFrontier, GraphConfig& config, ParseContext& context,
                    std::string& errorMessage) const;

    /**
     * @brief Parses a single segment (either a bare node or a parenthesised sub-graph).
     * @return @c true on success.
     */
    bool parseSegment(const std::string& text, const std::vector<std::string>& inputFrontier, std::vector<std::string>& outputFrontier, GraphConfig& config, ParseContext& context,
                      std::string& errorMessage) const;

    /**
     * @brief Tokenises a chain string into alternating node segments and link tokens.
     * @return @c true on success.
     */
    bool tokenizeChain(const std::string& text, std::vector<std::string>& segments, std::string& errorMessage) const;

    /**
     * @brief Splits @p text at @p delimiter characters that are at depth 0 (not inside parentheses).
     * @return Vector of top-level substrings.
     */
    std::vector<std::string> splitTopLevel(const std::string& text, char delimiter) const;

    /**
     * @brief Returns @c true if @p text is fully enclosed by matching parentheses.
     */
    bool isWrappedByParentheses(const std::string& text) const;

    /**
     * @brief Parses a single node segment (type name + optional parameters) into a @ref NodeConfig.
     * @return Parsed node configuration.
     */
    NodeConfig parseNode(const std::string& text, ParseContext& context, std::string& errorMessage) const;

    /**
     * @brief Adds @p node to @p config, connects it to @p inputFrontier and updates @p outputFrontier.
     * @return @c true on success.
     */
    bool addNodeAndConnect(const NodeConfig& node, const std::vector<std::string>& inputFrontier, std::vector<std::string>& outputFrontier, GraphConfig& config, ParseContext& context,
                           std::string& errorMessage) const;

    /** @brief Returns @c true if @p type is a registered sink type. */
    bool isSinkType(const std::string& type) const;

    /** @brief Parses a parameter string `key=val,key2=val2,...` into @p parameters. */
    void parseParameters(const std::string& text, ParameterSet& parameters) const;

    /** @brief Converts a raw parameter value string to a typed @ref ParameterValue. */
    ParameterValue parseParameterValue(const std::string& text) const;

    /** @brief Returns @p text with leading and trailing whitespace removed. */
    std::string trim(const std::string& text) const;
};
