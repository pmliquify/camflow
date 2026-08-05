// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "Application.hpp"

#include "core/Logger.hpp"
#include "convert/ImageConverter.hpp"
#include "parser/CLIPipelineParser.hpp"
#include "parser/JsonPipelineParser.hpp"
#include "pipeline/PipelineBuilder.hpp"
#include "processors/CCMProcessor.hpp"
#include "processors/CompositorProcessor.hpp"
#include "processors/DebayerProcessor.hpp"
#include "sinks/FileSink.hpp"
#include "sinks/LogSink.hpp"
#include "sinks/TCPSink.hpp"
#include "sources/FileSource.hpp"
#include "sources/V4L2Source.hpp"
#include "network/WebServer.hpp"
#include "version.h"

#include <opencv2/core/version.hpp>

#ifdef HAVE_GSTREAMER
#include "sources/NvArgusSource.hpp"
#endif

#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <thread>

Application::Application()
{
    registerNodes();
    registerConverters();
}

bool Application::hasFlag(int argc, char** argv, const std::string& shortOption, const std::string& longOption) const
{
    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];
        if (option == shortOption || (!longOption.empty() && option == longOption)) {
            return true;
        }
    }
    return false;
}

void Application::registerNodes()
{
    m_factory.registerType("filesrc", NodeKind::Source, []() { return std::make_unique<FileSource>(); });
    m_factory.registerType("compositor", NodeKind::Processor, []() { return std::make_unique<CompositorProcessor>(); });
    m_factory.registerType("debayer", NodeKind::Processor, []() { return std::make_unique<DebayerProcessor>(); });
    m_factory.registerType("ccm", NodeKind::Processor, []() { return std::make_unique<CCMProcessor>(); });
    m_factory.registerType("filesink", NodeKind::Sink, []() { return std::make_unique<FileSink>(); });
    m_factory.registerType("logsink", NodeKind::Sink, []() { return std::make_unique<LogSink>(); });
    m_factory.registerType("v4l2src", NodeKind::Source, []() { return std::make_unique<V4L2Source>(); });
#ifdef HAVE_GSTREAMER
    m_factory.registerType("nvargussrc", NodeKind::Source, []() { return std::make_unique<NvArgusSource>(); });
#endif
    m_factory.registerType("tcpsink", NodeKind::Sink, []() { return std::make_unique<TCPSink>(); });
}

void Application::registerConverters()
{
    m_converter = std::make_unique<ImageConverter>();
}

bool Application::hasHelp(int argc, char** argv) const
{
    return hasFlag(argc, argv, "", "--help");
}

bool Application::hasVersion(int argc, char** argv) const
{
    return hasFlag(argc, argv, "", "--version");
}

void Application::printVersion() const
{
    std::cout << "v" << CAMFLOW_VERSION << " | opencv " << CV_VERSION << " (" << CAMFLOW_GIT_COMMIT << ", " << CAMFLOW_BUILD_TIMESTAMP << ")\n";
}

bool Application::getArgumentValue(int argc, char** argv, const std::string& option, std::string& value) const
{
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == option && i + 1 < argc) {
            value = argv[i + 1];
            return true;
        }
    }
    return false;
}

bool Application::getPositionalPipelineArgument(int argc, char** argv, std::string& value) const
{
    for (int i = 1; i < argc; ++i) {
        const std::string current = argv[i];
        if (current == "-n" || current == "-G" || current == "--graph" || current == "--port" || current == "--device" || current == "--subdevices" || current == "-L" ||
            current == "--log-source") {
            ++i;
            continue;
        }
        if (current == "--rest-api") {
            continue;
        }
        if (current == "-v" || current == "--verbose") {
            if (i + 1 < argc) {
                const std::string next = argv[i + 1];
                if (!next.empty() && next.front() != '-') {
                    bool numeric = true;
                    for (char c : next) {
                        if (!std::isdigit(static_cast<unsigned char>(c))) {
                            numeric = false;
                            break;
                        }
                    }
                    if (numeric) {
                        ++i;
                    }
                }
            }
            continue;
        }
        if (current == "--help" || current == "--version" || current == "--debug") {
            continue;
        }
        if (!current.empty() && current.front() == '-') {
            continue;
        }
        value = current;
        return true;
    }
    return false;
}

bool Application::getOptionalPortValue(int argc, char** argv, const std::string& option, int defaultValue, int& value, bool& present) const
{
    present = false;
    value = defaultValue;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) != option) {
            continue;
        }
        present = true;
        if (i + 1 >= argc) {
            return true;
        }

        const std::string next = argv[i + 1];
        if (next.empty() || next.front() == '-') {
            return true;
        }

        try {
            size_t parsed = 0;
            long long result = std::stoll(next, &parsed, 10);
            if (parsed != next.size() || result <= 0 || result > 65535) {
                return false;
            }
            value = static_cast<int>(result);
            return true;
        } catch (...) {
            return false;
        }
    }
    return true;
}

bool Application::getArgumentIntValue(int argc, char** argv, const std::string& option, int& value) const
{
    std::string text;
    if (!getArgumentValue(argc, argv, option, text)) {
        return false;
    }

    try {
        size_t parsed = 0;
        long long result = std::stoll(text, &parsed, 10);
        if (parsed != text.size()) {
            return false;
        }
        if (result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max()) {
            return false;
        }
        value = static_cast<int>(result);
        return true;
    } catch (...) {
        return false;
    }
}

void Application::printSchema(const std::string& indent, const NodeSchema& schema, const ParameterSet* usedParameters) const
{
    if (!schema.parameters.empty()) {
        size_t parameterNameWidth = 0;
        for (const auto& parameter : schema.parameters) {
            parameterNameWidth = std::max(parameterNameWidth, parameter.name.size());
        }

        std::cout << indent << "Parameters:\n";
        for (const auto& parameter : schema.parameters) {
            bool used = usedParameters != nullptr && usedParameters->contains(parameter.name);
            std::cout << indent << "  " << (used ? "* " : "  ") << std::left << std::setw(static_cast<int>(parameterNameWidth + 2)) << parameter.name;
            std::cout << "default=" << std::setw(12) << parameterValueToString(parameter.defaultValue);
            std::cout << parameter.description;
            if (!parameter.options.empty()) {
                std::cout << " [";
                for (size_t i = 0; i < parameter.options.size(); ++i) {
                    if (i) {
                        std::cout << ", ";
                    }
                    std::cout << parameter.options[i];
                }
                std::cout << "]";
            }
            std::cout << "\n";
        }
    }

    if (!schema.inputs.empty()) {
        std::cout << indent << "Inputs:\n";
        for (const auto& input : schema.inputs) {
            std::cout << indent << "    " << input.name << " (" << input.dataType << ")";
            if (input.allowMultipleBindings) {
                std::cout << " [multi]";
            }
            if (!input.description.empty()) {
                std::cout << " - " << input.description;
            }
            std::cout << "\n";
        }
    }

    if (!schema.outputs.empty()) {
        std::cout << indent << "Outputs:\n";
        for (const auto& output : schema.outputs) {
            std::cout << indent << "    " << output.name << " (" << output.dataType << ")";
            if (!output.description.empty()) {
                std::cout << " - " << output.description;
            }
            std::cout << "\n";
        }
    }
}

void Application::printNodeList(NodeKind kind, const std::string& title) const
{
    std::cout << title << ":\n";
    for (const auto& type : m_factory.registeredTypes(kind)) {
        std::cout << "  " << std::left << std::setw(26) << type << m_factory.description(type) << "\n";
    }
    std::cout << "\n";
}

void Application::printHelp(const char* executableName, const GraphConfig& config, const std::string& pipelineText) const
{
    std::cout << "Usage: " << executableName << " [OPTION...] PIPELINE                   \n";
    std::cout << "       " << executableName << " [OPTION...] -G FILE                     \n";
    std::cout << CAMFLOW_VERSION << " | " << CV_VERSION << " (git " << CAMFLOW_GIT_COMMIT << ", " << CAMFLOW_BUILD_TIMESTAMP << ")\n";
    std::cout << "                                                                       \n";
    std::cout << "Options:                                                               \n";
    std::cout << "      --help                 Show this help                            \n";
    std::cout << "      --version              Show version                              \n";
    std::cout << "  -G, --graph FILE           Load graph from json file                 \n";
    std::cout << "      PIPELINE               Pipeline expression (required without -G) \n";
    std::cout << "  -n MAX_FRAMES              Process at most MAX_FRAMES frames         \n";
    std::cout << "                             (0 = unlimited)                           \n";
    std::cout << "  -s, --simple-pipeline      Use linear execution (ignores graph edges)\n";
    std::cout << "  -p, --profile              Enable node execution profiling report     \n";
    std::cout << "  -v, --verbose [LEVEL]      Log WebServer requests (default LEVEL=1)   \n";
    std::cout << "                             LEVEL=2 truncates bodies to 80 chars       \n";
    std::cout << "                             LEVEL=3 logs full API request/response bodies\n";
    std::cout << "                                     except /api/runtime polling          \n";
    std::cout << "                             LEVEL=4 logs full API request/response bodies\n";
    std::cout << "      --debug                Show timestamp, log type and source file  \n";
    std::cout << "  -L, --log-source LIST      Console sources: runtime,node,api,kernel \n";
    std::cout << "                             Supports all, none and -source (default: node)\n";
    std::cout << "      --rest-api             Enable REST API server in pipeline mode     \n";
    std::cout << "      --port PORT            UI/REST server port (default: 8000)         \n";
    std::cout << "      --device PATH          V4L2 device for auto UI mode (default: /dev/video3)\n";
    std::cout << "      --subdevices LIST      Comma-separated V4L2 subdevices for auto UI mode\n";
    std::cout << "                             (default: /dev/v4l-subdev3)\n";
    std::cout << "                                                                       \n";
    std::cout << "Simple pipeline syntax:                             \n";
    std::cout << "  v4l2src(device=/dev/video0) -> tcpsink(ip=127.0.0.1,port=9000)\n\n";
    std::cout << "Input bindings are configured in node arguments as <input>=<nodeId>.<output>.\n";
    std::cout << "Example: compositor(image=cam0.image,cam1.image, xpos=0,100)\n\n";

    if (pipelineText.empty() && config.empty()) {
        std::cout << "Available nodes:\n\n";
        printNodeList(NodeKind::Source, "Sources");
        printNodeList(NodeKind::Processor, "Processors");
        printNodeList(NodeKind::Sink, "Sinks");
        return;
    }

    std::cout << "Selected pipeline nodes:\n";
    std::cout << "  Parameters marked with '*' are explicitly used in the pipeline expression or graph file.\n\n";
    for (const auto& node : config.nodes()) {
        std::cout << "  " << node.id << ":" << node.type << "\n";
        auto selectedNode = m_factory.create(node.type);
        NodeSchema schema;
        if (selectedNode) {
            selectedNode->configure(node.parameters);
            schema = selectedNode->schema();
        } else {
            schema = m_factory.schema(node.type);
        }
        if (!schema.parameters.empty() || !schema.inputs.empty() || !schema.outputs.empty()) {
            printSchema("    ", schema, &node.parameters);
        }
        std::cout << "\n";
    }
}

int Application::runUiMode(int argc, char** argv)
{
    int verboseLevel = 0;
    if (!getVerboseLevel(argc, argv, verboseLevel)) {
        LOG_ERROR("Invalid value for -v/--verbose: must be an integer >= 1");
        return 1;
    }

    int serverPort = 8000;
    bool portProvided = false;
    if (!getOptionalPortValue(argc, argv, "--port", 8000, serverPort, portProvided)) {
        LOG_ERROR("Invalid value for --port");
        return 1;
    }

    std::string device = "/dev/video3";
    std::string subdevices = "/dev/v4l-subdev3";
    getArgumentValue(argc, argv, "--device", device);
    getArgumentValue(argc, argv, "--subdevices", subdevices);

    GraphConfig config;
    NodeConfig sourceNode;
    sourceNode.id = "v4l2src0";
    sourceNode.type = "v4l2src";
    sourceNode.parameters.set("device", device);
    sourceNode.parameters.set("subdevices", subdevices);
    config.addNode(sourceNode);

    PipelineBuilder builder(m_factory, m_converter.get());
    auto pipeline = builder.build(config, false);
    if (!pipeline) {
        return 1;
    }

    LOG_INFO("Pipeline implementation: PipelineGraph (graph)");

    if (!pipeline->init()) {
        return 1;
    }

    // UI mode starts in stopped state: keep device handles initialized but do not
    // start streaming until the user requests runtime start.
    pipeline->setStopped(true);

    m_controller.setIncrementalNodeFactory([this](const NodeConfig& nodeConfig) -> NodePtr {
        NodePtr node = m_factory.create(nodeConfig.type);
        if (!node) {
            return nullptr;
        }
        node->setId(nodeConfig.id);
        node->setImageConverter(m_converter.get());
        node->configure(nodeConfig.parameters);
        return node;
    });

    m_controller.setPipelineRebuildHandler([this](const GraphConfig& rebuildConfig, bool stopped) -> std::unique_ptr<IPipeline> {
        PipelineBuilder rebuildBuilder(m_factory, m_converter.get());
        auto rebuiltPipeline = rebuildBuilder.build(rebuildConfig, false);
        if (!rebuiltPipeline) {
            return nullptr;
        }
        if (!rebuiltPipeline->init()) {
            return nullptr;
        }
        if (!stopped) {
            if (!rebuiltPipeline->start()) {
                return nullptr;
            }
        }
        rebuiltPipeline->setStopped(stopped);
        return rebuiltPipeline;
    });

    m_controller.setPipeline(std::move(pipeline), config);
    if (!m_controller.setStopped(true)) {
        LOG_ERROR("Could not set initial stopped state for UI mode");
        return 1;
    }
    // Materialize the stopped lifecycle transition once so subsequent start toggles
    // trigger node->start() deterministically.
    m_controller.runFrames(1);

    WebServer server(m_factory, "v4l2src0");
    if (!server.start(static_cast<uint16_t>(serverPort), m_controller, verboseLevel)) {
        LOG_ERROR("Could not start UI server on port " + std::to_string(serverPort));
        return 1;
    }

    LOG_INFO("camflow UI mode active. Open http://127.0.0.1:" + std::to_string(serverPort));

    const auto retryDelay = std::chrono::milliseconds(500);
    int totalFrames = 0;
    while (true) {
        try {
            const int frames = m_controller.runFrames(1);
            if (m_controller.isStopped()) {
                std::this_thread::sleep_for(retryDelay);
                continue;
            }
            totalFrames += frames;
            if (frames <= 0) {
                LOG_WARNING("UI pipeline run iteration produced no frames (total=" + std::to_string(totalFrames) + "). Retrying in 500 ms.");
                std::this_thread::sleep_for(retryDelay);
            }
        } catch (const std::exception& exception) {
            LOG_ERROR(std::string("UI pipeline exception: ") + exception.what() + ". Retrying in 500 ms.");
            std::this_thread::sleep_for(retryDelay);
        } catch (...) {
            LOG_ERROR("UI pipeline exception: unknown error. Retrying in 500 ms.");
            std::this_thread::sleep_for(retryDelay);
        }
    }

    return 0;
}

int Application::run(int argc, char** argv)
{
    if (handleVersion(argc, argv)) {
        return 0;
    }

    if (!configureLogger(argc, argv)) {
        return 1;
    }

    if (handleHelp(argc, argv, nullptr, nullptr)) {
        return 0;
    }

    std::string pipelineText;
    int maxFrames = 0;
    if (!parseCli(argc, argv, pipelineText, maxFrames)) {
        return 1;
    }

    std::string graphFile;
    getArgumentValue(argc, argv, "--graph", graphFile);
    if (graphFile.empty()) {
        getArgumentValue(argc, argv, "-G", graphFile);
    }

    GraphConfig config;
    if (!parseGraph(graphFile, pipelineText, config)) {
        return 1;
    }

    if (handleHelp(argc, argv, &config, &pipelineText)) {
        return 0;
    }

    if (config.empty()) {
        return runUiMode(argc, argv);
    }

    IPipeline* runningPipeline = buildPipeline(argc, argv, config);
    if (runningPipeline == nullptr) {
        return 1;
    }

    std::unique_ptr<WebServer> server;
    if (!startRestApi(argc, argv, server)) {
        return 1;
    }

    return cleanupRun(runningPipeline, server, maxFrames);
}

bool Application::handleVersion(int argc, char** argv) const
{
    if (!hasVersion(argc, argv)) {
        return false;
    }
    printVersion();
    return true;
}

bool Application::configureLogger(int argc, char** argv) const
{
    logger().setVerbose(hasFlag(argc, argv, "", "--debug"));

    uint32_t sourceMask = logSourceMask(LogSource::Node);
    std::string errorMessage;
    if (!getLogSourceMask(argc, argv, sourceMask, errorMessage)) {
        std::cerr << errorMessage << std::endl;
        return false;
    }
    logger().setConsoleSourceMask(sourceMask);
    return true;
}

bool Application::getLogSourceMask(int argc, char** argv, uint32_t& sourceMask, std::string& errorMessage) const
{
    bool present = false;
    sourceMask = logSourceMask(LogSource::Node);
    errorMessage.clear();

    auto maskForName = [](const std::string& name, uint32_t& mask) {
        if (name == "runtime") {
            mask = logSourceMask(LogSource::Runtime);
            return true;
        }
        if (name == "node") {
            mask = logSourceMask(LogSource::Node);
            return true;
        }
        if (name == "api") {
            mask = logSourceMask(LogSource::Api);
            return true;
        }
        if (name == "kernel") {
            mask = logSourceMask(LogSource::Kernel);
            return true;
        }
        return false;
    };

    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];
        if (option != "-L" && option != "--log-source") {
            continue;
        }
        if (i + 1 >= argc) {
            errorMessage = "Missing value for " + option + ". Expected runtime, node, api, kernel, all, or none.";
            return false;
        }
        if (!present) {
            sourceMask = 0;
            present = true;
        }

        std::istringstream values(argv[++i]);
        std::string token;
        while (std::getline(values, token, ',')) {
            token.erase(token.begin(), std::find_if(token.begin(), token.end(), [](unsigned char c) { return std::isspace(c) == 0; }));
            token.erase(std::find_if(token.rbegin(), token.rend(), [](unsigned char c) { return std::isspace(c) == 0; }).base(), token.end());
            if (token.empty()) {
                errorMessage = "Empty log source in " + option + ". Expected runtime, node, api, kernel, all, or none.";
                return false;
            }

            bool remove = token.size() > 1 && token.front() == '-';
            const std::string name = remove ? token.substr(1) : token;
            if (name == "all") {
                sourceMask = remove ? 0 : allLogSourceMask();
                continue;
            }
            if (name == "none") {
                sourceMask = remove ? allLogSourceMask() : 0;
                continue;
            }

            uint32_t tokenMask = 0;
            if (!maskForName(name, tokenMask)) {
                errorMessage = "Unknown log source '" + name + "'. Expected runtime, node, api, kernel, all, or none.";
                return false;
            }
            if (remove) {
                sourceMask &= ~tokenMask;
            } else {
                sourceMask |= tokenMask;
            }
        }
    }

    return true;
}

bool Application::handleHelp(int argc, char** argv, const GraphConfig* config, const std::string* pipelineText) const
{
    if (!hasHelp(argc, argv)) {
        return false;
    }

    if (config != nullptr && pipelineText != nullptr) {
        printHelp(argv[0], *config, *pipelineText);
        return true;
    }

    GraphConfig emptyConfig;
    const std::string emptyPipelineText;
    printHelp(argv[0], emptyConfig, emptyPipelineText);
    return true;
}

bool Application::parseCli(int argc, char** argv, std::string& pipelineText, int& maxFrames) const
{
    int verboseLevel = 0;
    if (!getVerboseLevel(argc, argv, verboseLevel)) {
        LOG_ERROR("Invalid value for -v/--verbose: must be an integer >= 1");
        return false;
    }

    getPositionalPipelineArgument(argc, argv, pipelineText);

    std::string maxFramesText;
    if (!getArgumentValue(argc, argv, "-n", maxFramesText)) {
        return true;
    }

    if (!getArgumentIntValue(argc, argv, "-n", maxFrames)) {
        LOG_ERROR("Invalid value for -n: " + maxFramesText);
        return false;
    }
    if (maxFrames < 0) {
        LOG_ERROR("Invalid value for -n: must be >= 0");
        return false;
    }

    return true;
}

bool Application::getVerboseLevel(int argc, char** argv, int& value) const
{
    value = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string current = argv[i];
        if (current != "-v" && current != "--verbose") {
            continue;
        }

        value = 1;
        if (i + 1 >= argc) {
            return true;
        }

        const std::string next = argv[i + 1];
        if (next.empty() || next.front() == '-') {
            return true;
        }

        try {
            size_t parsed = 0;
            const int parsedValue = std::stoi(next, &parsed, 10);
            if (parsed != next.size() || parsedValue < 1) {
                return false;
            }
            value = parsedValue;
            ++i;
        } catch (...) {
            return false;
        }
    }
    return true;
}

bool Application::parseGraph(const std::string& graphFile, const std::string& pipelineText, GraphConfig& config) const
{
    std::string errorMessage;
    if (!graphFile.empty()) {
        JsonPipelineParser jsonParser;
        config = GraphConfig();
        if (!jsonParser.parseFile(graphFile, config, errorMessage)) {
            LOG_ERROR(errorMessage);
            return false;
        }
        return true;
    }

    if (!pipelineText.empty()) {
        CLIPipelineParser expressionParser;
        config = GraphConfig();
        if (!expressionParser.parse(pipelineText, config, errorMessage)) {
            LOG_ERROR(errorMessage);
            return false;
        }
    }

    return true;
}

IPipeline* Application::buildPipeline(int argc, char** argv, const GraphConfig& config)
{
    const bool useSimplePipeline = hasFlag(argc, argv, "-s", "--simple-pipeline");
    const bool profilingEnabled = hasFlag(argc, argv, "-p", "--profile");

    m_controller.setIncrementalNodeFactory([this](const NodeConfig& nodeConfig) -> NodePtr {
        NodePtr node = m_factory.create(nodeConfig.type);
        if (!node) {
            return nullptr;
        }
        node->setId(nodeConfig.id);
        node->setImageConverter(m_converter.get());
        node->configure(nodeConfig.parameters);
        return node;
    });

    m_controller.setPipelineRebuildHandler([this, useSimplePipeline, profilingEnabled](const GraphConfig& rebuildConfig, bool stopped) -> std::unique_ptr<IPipeline> {
        PipelineBuilder rebuildBuilder(m_factory, m_converter.get());
        auto rebuiltPipeline = rebuildBuilder.build(rebuildConfig, useSimplePipeline);
        if (!rebuiltPipeline) {
            return nullptr;
        }
        rebuiltPipeline->setProfilingEnabled(profilingEnabled);
        if (!rebuiltPipeline->init()) {
            return nullptr;
        }
        if (!stopped) {
            if (!rebuiltPipeline->start()) {
                return nullptr;
            }
        }
        rebuiltPipeline->setStopped(stopped);
        return rebuiltPipeline;
    });

    PipelineBuilder builder(m_factory, m_converter.get());
    auto pipeline = builder.build(config, useSimplePipeline);
    if (!pipeline) {
        return nullptr;
    }

    LOG_INFO(std::string("Pipeline implementation: ") + (useSimplePipeline ? "Pipeline (linear)" : "PipelineGraph (graph)"));

    pipeline->setProfilingEnabled(profilingEnabled);

    if (!pipeline->init()) {
        return nullptr;
    }

    if (!pipeline->start()) {
        return nullptr;
    }

    IPipeline* runningPipeline = pipeline.get();
    m_controller.setPipeline(std::move(pipeline), config);
    return runningPipeline;
}

bool Application::startRestApi(int argc, char** argv, std::unique_ptr<WebServer>& server)
{
    const bool restApiEnabled = hasFlag(argc, argv, "", "--rest-api");

    if (!restApiEnabled) {
        return true;
    }

    int restApiPort = 8000;
    bool portProvided = false;
    if (!getOptionalPortValue(argc, argv, "--port", 8000, restApiPort, portProvided)) {
        LOG_ERROR("Invalid value for --port");
        return false;
    }

    int verboseLevel = 0;
    if (!getVerboseLevel(argc, argv, verboseLevel)) {
        LOG_ERROR("Invalid value for -v/--verbose: must be an integer >= 1");
        return false;
    }

    server = std::make_unique<WebServer>(m_factory, std::string());
    if (!server->start(static_cast<uint16_t>(restApiPort), m_controller, verboseLevel)) {
        LOG_ERROR("Could not start REST server on port " + std::to_string(restApiPort));
        server.reset();
        return false;
    }
    return true;
}

int Application::cleanupRun(IPipeline* runningPipeline, std::unique_ptr<WebServer>& server, int maxFrames) const
{
    int frames = runningPipeline->run(maxFrames);
    runningPipeline->stop();
    runningPipeline->shutdown();
    if (server) {
        server->stop();
    }

    LOG_INFO("Processed frames: " + std::to_string(frames));
    return 0;
}
