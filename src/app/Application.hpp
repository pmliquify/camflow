// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include "core/RuntimeController.hpp"
#include "image/IImageConverter.hpp"
#include "pipeline/GraphConfig.hpp"
#include "pipeline/NodeFactory.hpp"

class WebServer;

#include <memory>
#include <string>

/**
 * @brief Top-level application class that owns all runtime components and implements the CLI entry point.
 *
 * Application wires together the @ref NodeFactory, a single @ref IImageConverter,
 * @ref RuntimeController and @ref PipelineBuilder. It parses command-line arguments,
 * builds the pipeline from either a JSON graph file or a CLI pipeline expression,
 * starts the optional REST server and drives the processing loop.
 *
 * ### CLI argument handling
 * | Flag / Argument         | Description                                                  |
 * |-------------------------|--------------------------------------------------------------|
 * | `<pipeline>`            | Positional: CLI DSL expression (used unless `-G` is given).  |
 * | `-G`, `--graph`         | Path to a JSON graph file.                                   |
 * | `-n MAX_FRAMES`         | Maximum number of frames to process (0 = unlimited).        |
 * | `-s`, `--simple-pipeline` | Force linear @ref Pipeline execution.               |
 * | `-p`, `--profile`       | Enable node execution profiling report.                      |
 * | `-v`, `--verbose [N]`   | WebServer verbosity (`1`=requests, `2`=truncated bodies, `3`=full bodies except `/api/runtime`, `4`=full bodies incl. runtime polling). |
 * | `-L`, `--log-source LIST` | Console log sources (`runtime,node,api,kernel`, default `node`). |
 * | `--debug`               | Enable detailed logger output (timestamp, level, file/line). |
 * | `--rest-api`            | Enable REST API for pipeline mode (disabled by default).      |
 * | `--port PORT`           | Port used by UI/REST server (default: 8080).                  |
 * | `--device PATH`         | Initial V4L2 source device for the built-in web UI.          |
 * | `--subdevices LIST`     | Initial V4L2 subdevices for the built-in web UI.             |
 * | `-h`, `--help`          | Print usage and node list.                                   |
 * | `--version`             | Print version string.                                        |
 *
 * ### Node registration
 * All built-in node types are registered in @ref registerNodes. NvArgus nodes are
 * registered when GStreamer support is enabled.
 *
 * @see PipelineBuilder
 * @see NodeFactory
 * @see RuntimeController
 * @see WebServer
 */
class Application
{
public:
    /// Constructs the application and pre-registers all built-in node types and the converter.
    Application();

    /**
     * @brief Parses arguments, builds the pipeline and runs the processing loop.
     *
     * This is the main entry point called from `main()`. It performs all argument
     * parsing, pipeline construction and execution. Returns the process exit code.
     *
     * @param argc Argument count from `main()`.
     * @param argv Argument vector from `main()`.
     * @return @c 0 on success; non-zero on error.
     */
    int run(int argc, char** argv);

private:
    /** @brief Registers all built-in runtime node types with the @ref NodeFactory. */
    void registerNodes();

    /** @brief Registers the available image converter. */
    void registerConverters();

    /** @brief Runs the dedicated single-node UI mode (auto when no graph/pipeline is provided). */
    int runUiMode(int argc, char** argv);

    /** @brief Section 1: handles --version and returns true when execution should stop. */
    bool handleVersion(int argc, char** argv) const;

    /** @brief Section 2: configures logger formatting and console source filtering. */
    bool configureLogger(int argc, char** argv) const;

    /** @brief Parses all `-L`/`--log-source` selections into a console source mask. */
    bool getLogSourceMask(int argc, char** argv, uint32_t& sourceMask, std::string& errorMessage) const;

    /**
     * @brief Section 3: handles help output.
     *
     * If @p config and @p pipelineText are null, generic help output is produced.
     * If both are non-null, full help output is produced with the selected pipeline.
     */
    bool handleHelp(int argc, char** argv, const GraphConfig* config, const std::string* pipelineText) const;

    /** @brief Section 4: parses CLI values needed before graph parsing. */
    bool parseCli(int argc, char** argv, std::string& pipelineText, int& maxFrames) const;

    /** @brief Section 5: parses graph input from file or pipeline expression. */
    bool parseGraph(const std::string& graphFile, const std::string& pipelineText, GraphConfig& config) const;

    /** @brief Section 6: builds, configures and initializes the runtime pipeline. */
    IPipeline* buildPipeline(int argc, char** argv, const GraphConfig& config);

    /** @brief Section 7: parses and starts optional REST API server. */
    bool startRestApi(int argc, char** argv, std::unique_ptr<WebServer>& server);

    /** @brief Section 8: runs the pipeline and performs shutdown cleanup. */
    int cleanupRun(IPipeline* runningPipeline, std::unique_ptr<WebServer>& server, int maxFrames) const;

    /**
     * @brief Checks for the presence of a boolean flag in the argument list.
     * @param argc Argument count from `main()`.
     * @param argv Argument vector from `main()`.
     * @param shortOption Short form (e.g. `"-p"`).
     * @param longOption  Long form (e.g. `"--profile"`); empty string if none.
     * @return @c true if either form is present.
     */
    bool hasFlag(int argc, char** argv, const std::string& shortOption, const std::string& longOption = std::string()) const;

    /** @brief Returns @c true if `-h` or `--help` is present in the arguments. */
    bool hasHelp(int argc, char** argv) const;

    /** @brief Returns @c true if `--version` is present in the arguments. */
    bool hasVersion(int argc, char** argv) const;

    /** @brief Prints the application version string to stdout. */
    void printVersion() const;

    /**
     * @brief Reads the string value following the given option flag.
     * @param argc Argument count from `main()`.
     * @param argv Argument vector from `main()`.
     * @param option The flag to look for (e.g. `"-G"`).
     * @param value  Output parameter; receives the string that follows @p option.
     * @return @c true if @p option was found and has a following value.
     */
    bool getArgumentValue(int argc, char** argv, const std::string& option, std::string& value) const;

    /**
     * @brief Finds the positional pipeline expression argument.
     *
     * The pipeline expression is any argument that does not start with `-`
     * and is not the value of another option.
     *
     * @param argc Argument count from `main()`.
     * @param argv Argument vector from `main()`.
     * @param value Output parameter; receives the pipeline expression string.
     * @return @c true if a positional argument was found.
     */
    bool getPositionalPipelineArgument(int argc, char** argv, std::string& value) const;

    /**
     * @brief Reads the integer value following the given option flag.
     * @param argc Argument count from `main()`.
     * @param argv Argument vector from `main()`.
     * @param option The flag to look for.
     * @param value  Output parameter; receives the parsed integer.
     * @return @c true if @p option was found and its value could be parsed as an integer.
     */
    bool getArgumentIntValue(int argc, char** argv, const std::string& option, int& value) const;

    /**
     * @brief Reads an optional integer value that may follow the given flag.
     *
     * If the flag is present without an integer argument, @p defaultValue is used.
     *
     * @return @c true when the option is valid; @c false on parse or range errors.
     */
    bool getOptionalPortValue(int argc, char** argv, const std::string& option, int defaultValue, int& value, bool& present) const;

    /**
     * @brief Reads the optional WebServer verbosity level.
     *
     * `-v` or `--verbose` without a value selects level 1.
     * `-v 2` / `--verbose 2` selects level 2.
     *
     * @param argc Argument count from `main()`.
     * @param argv Argument vector from `main()`.
     * @param value Output verbosity level (0 when not present).
     * @return @c true on success; @c false on invalid numeric input.
     */
    bool getVerboseLevel(int argc, char** argv, int& value) const;

    /**
     * @brief Prints the full help text including the node list and current pipeline.
     * @param executableName  Name of the binary as shown in the usage line.
     * @param config          Current @ref GraphConfig (for displaying active nodes/edges).
     * @param pipelineText    Current pipeline expression string.
     */
    void printHelp(const char* executableName, const GraphConfig& config, const std::string& pipelineText) const;

    /**
     * @brief Prints the parameter schema of a node type in indented format.
     * @param indent          Indentation prefix for each line.
     * @param schema          The parameter schema to display.
     * @param usedParameters  If non-null, currently active values are shown next to defaults.
     */
    void printSchema(const std::string& indent, const NodeSchema& schema, const ParameterSet* usedParameters) const;

    /**
     * @brief Prints all registered node types of the given kind under a section heading.
     * @param kind  The @ref NodeKind to filter by.
     * @param title Section heading string.
     */
    void printNodeList(NodeKind kind, const std::string& title) const;

    NodeFactory m_factory;                        ///< Registry of all registered node types.
    std::unique_ptr<IImageConverter> m_converter; ///< Shared image converter for the pipeline.
    RuntimeController m_controller;               ///< Owns the running pipeline and exposes control API.
};
