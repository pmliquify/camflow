// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

/**
 * @brief Severity levels for log messages.
 *
 * LogType controls both the prefix shown in verbose mode and the output channel.
 * In non-verbose mode all levels produce only the message text on stdout.
 * In debug mode (enabled via `--debug`) each line is prefixed with
 * a timestamp, the level label and the source file name.
 */
enum class LogType
{
    Debug,   ///< Detailed diagnostic information; only relevant for development.
    Info,    ///< Normal operational messages (pipeline start, frame counts, reports).
    Warning, ///< Recoverable issues that do not stop processing.
    Error    ///< Fatal or serious errors that typically terminate processing.
};

/**
 * @brief Singleton application logger with optional verbose output.
 *
 * Logger provides thread-safe log output for the CamFlow runtime. All internal
 * components use the convenience macros @ref LOG_DEBUG, @ref LOG_INFO,
 * @ref LOG_WARNING and @ref LOG_ERROR rather than calling the logger directly.
 *
 * ### Debug mode
 * By default only the raw message text is written to stdout. When debug mode
 * is enabled via @ref setVerbose every line additionally shows:
 * - An ISO-8601 timestamp
 * - The log level label
 * - The source file name and line number
 *
 * ### Thread safety
 * @ref log serialises all output through an internal `std::mutex` so that log
 * messages from concurrent pipeline nodes do not interleave.
 *
 * ### Usage
 * Use the provided macros; do not call @ref log directly:
 * @code
 * LOG_INFO("Pipeline started");
 * LOG_ERROR("Failed to open device: " + devicePath);
 * @endcode
 *
 * @see logger()
 */
class Logger
{
public:
    struct LogRecord
    {
        LogType type = LogType::Info;
        std::string source = "runtime";
        std::string sourceTag;
        std::string file;
        int line = 0;
        std::string message;
        std::string rendered;
        uint64_t timestampMs = 0;
    };

    using LogListener = std::function<void(const LogRecord&)>;

    Logger();
    ~Logger();

    /**
     * @brief Enables or disables debug output (timestamp, level, file/line).
     * @param verbose @c true to enable debug output.
     */
    void setVerbose(bool verbose);

    /**
     * @brief Registers a listener that receives each emitted log record.
     * @return Stable listener id used for removal.
     */
    size_t addListener(LogListener listener);

    /**
     * @brief Unregisters a previously registered log listener.
     */
    void removeListener(size_t listenerId);

    /**
     * @brief Returns up to @p maxCount most recent log records in chronological order.
     */
    std::vector<LogRecord> history(size_t maxCount) const;

    /**
     * @brief Writes a log message to stdout.
     *
     * Thread-safe. In debug mode prefixes the message with an ISO-8601
     * timestamp, the level label and the file:line location.
     *
     * @param type    Severity level of the message.
     * @param file    Source file path (typically from `__FILE__`).
     * @param line    Source line number (typically from `__LINE__`).
     * @param message The message text to log.
     */
    void log(LogType type, const std::string& file, int line, const std::string& message);

private:
    struct ListenerEntry
    {
        size_t id = 0;
        LogListener callback;
    };

    void publishRecord(LogRecord record);
    void startKernelReader();
    void stopKernelReader();
    void kernelReaderLoop();

    mutable std::mutex m_mutex; ///< Serialises concurrent log calls.
    bool m_verbose = false;     ///< Whether verbose prefix output is active.
    std::deque<LogRecord> m_history;
    std::vector<ListenerEntry> m_listeners;
    size_t m_nextListenerId = 1;
    std::atomic_bool m_kernelThreadRunning{false};
    std::thread m_kernelThread;
    int m_kernelFd = -1;
};

/**
 * @brief Returns the global application logger singleton.
 * @return Reference to the process-wide @ref Logger instance.
 */
Logger& logger();

/** @brief Logs a DEBUG-level message with source location. */
#define LOG_DEBUG(message) logger().log(LogType::Debug, __FILE__, __LINE__, (message))

/** @brief Logs an INFO-level message with source location. */
#define LOG_INFO(message) logger().log(LogType::Info, __FILE__, __LINE__, (message))

/** @brief Logs a WARNING-level message with source location. */
#define LOG_WARNING(message) logger().log(LogType::Warning, __FILE__, __LINE__, (message))

/** @brief Logs an ERROR-level message with source location. */
#define LOG_ERROR(message) logger().log(LogType::Error, __FILE__, __LINE__, (message))
