// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#include "Logger.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <ctime>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <poll.h>
#include <sstream>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <sstream>
#include <unistd.h>

namespace
{

const char* colorReset()
{
    return "\033[0m";
}

const char* colorForSource(const std::string& file)
{
    if (file == "kernel") {
        return "\033[37m"; // Light gray
    }
    if (file.find("/runtime/network/WebServer.cpp") != std::string::npos || file.find("/runtime/network/RestApiInterface.cpp") != std::string::npos ||
        file.find("/runtime/network/WebInterface.cpp") != std::string::npos) {
        return "\033[34m"; // Blue
    }
    if (file.find("/nodes/") != std::string::npos) {
        return "\033[33m"; // Yellow
    }
    if (file.find("/runtime/") != std::string::npos || file.find("/app/") != std::string::npos || file.find("/convert/") != std::string::npos) {
        return "\033[32m"; // Green
    }
    return "";
}

std::string sourceForFile(const std::string& file)
{
    if (file == "kernel") {
        return "kernel";
    }
    if (file.find("/runtime/network/WebServer.cpp") != std::string::npos || file.find("/runtime/network/RestApiInterface.cpp") != std::string::npos ||
        file.find("/runtime/network/WebInterface.cpp") != std::string::npos) {
        return "api";
    }
    if (file.find("/nodes/") != std::string::npos) {
        return "node";
    }
    if (file.find("/runtime/") != std::string::npos || file.find("/app/") != std::string::npos || file.find("/convert/") != std::string::npos) {
        return "runtime";
    }
    return "runtime";
}

const char* logTypeName(LogType type)
{
    switch (type) {
    case LogType::Debug:
        return "DEBUG";
    case LogType::Info:
        return "INFO";
    case LogType::Warning:
        return "WARN";
    case LogType::Error:
        return "ERROR";
    }
    return "INFO";
}

uint64_t nowMs()
{
    const auto now = std::chrono::system_clock::now();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
}

std::string formatRenderedLine(LogType type, bool verbose, const std::string& source, const std::string& sourceTag, const std::string& file, int line, const std::string& message)
{
    if (!verbose) {
        return message;
    }

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

    std::tm tmValue;
#if defined(_WIN32)
    localtime_s(&tmValue, &time);
#else
    localtime_r(&time, &tmValue);
#endif

    std::ostringstream stream;
    stream << std::put_time(&tmValue, "%Y-%m-%d %H:%M:%S") << "." << std::setw(3) << std::setfill('0') << ms << " [" << logTypeName(type) << "] " << message;
    if (source == "kernel") {
        stream << " (" << (sourceTag.empty() ? "kernel:0" : sourceTag) << ")";
    } else {
        stream << " (" << file << ":" << line << ")";
    }
    return stream.str();
}

LogType logTypeFromKernelLevel(int level)
{
    if (level <= 3) {
        return LogType::Error;
    }
    if (level == 4) {
        return LogType::Warning;
    }
    if (level == 7) {
        return LogType::Debug;
    }
    return LogType::Info;
}

bool parseKernelLine(const std::string& line, int& level, int& facility, std::string& message)
{
    const size_t separator = line.find(';');
    if (separator == std::string::npos) {
        level = 6;
        facility = 0;
        message = line;
        return false;
    }

    const std::string prefix = line.substr(0, separator);
    message = line.substr(separator + 1);

    const size_t firstComma = prefix.find(',');
    const std::string priorityText = firstComma == std::string::npos ? prefix : prefix.substr(0, firstComma);
    try {
        const int priorityValue = std::stoi(priorityText);
        level = priorityValue & 0x7;
        facility = priorityValue >> 3;
    } catch (...) {
        level = 6;
        facility = 0;
    }
    return true;
}

} // namespace

Logger& logger()
{
    static Logger instance;
    return instance;
}

Logger::Logger() :
    m_kernelThreadRunning(false)
{
    startKernelReader();
}

Logger::~Logger()
{
    stopKernelReader();
}

void Logger::setVerbose(bool verbose)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_verbose = verbose;
}

size_t Logger::addListener(LogListener listener)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const size_t id = m_nextListenerId++;
    m_listeners.push_back(ListenerEntry{id, std::move(listener)});
    return id;
}

void Logger::removeListener(size_t listenerId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_listeners.erase(std::remove_if(m_listeners.begin(), m_listeners.end(), [listenerId](const ListenerEntry& entry) { return entry.id == listenerId; }), m_listeners.end());
}

std::vector<Logger::LogRecord> Logger::history(size_t maxCount) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<LogRecord> records;
    const size_t count = std::min(maxCount, m_history.size());
    records.reserve(count);
    const size_t start = m_history.size() - count;
    size_t index = 0;
    for (const auto& record : m_history) {
        if (index++ < start) {
            continue;
        }
        records.push_back(record);
    }
    return records;
}

void Logger::publishRecord(LogRecord record)
{
    std::vector<LogListener> listeners;
    bool verbose = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        verbose = m_verbose;
        record.timestampMs = nowMs();
        if (record.rendered.empty()) {
            record.rendered = formatRenderedLine(record.type, verbose, record.source, record.sourceTag, record.file, record.line, record.message);
        }
        m_history.push_back(record);
        while (m_history.size() > 200) {
            m_history.pop_front();
        }
        listeners.reserve(m_listeners.size());
        for (const auto& entry : m_listeners) {
            listeners.push_back(entry.callback);
        }
    }

    const char* color = colorForSource(record.source == "kernel" ? std::string("kernel") : record.file);
    const bool useColor = color[0] != '\0';
    if (useColor) {
        std::cout << color;
    }
    std::cout << record.rendered;
    if (useColor) {
        std::cout << colorReset();
    }
    std::cout << std::endl;

    for (const auto& listener : listeners) {
        if (listener) {
            listener(record);
        }
    }
}

void Logger::log(LogType type, const std::string& file, int line, const std::string& message)
{
    LogRecord record;
    record.type = type;
    record.source = sourceForFile(file);
    record.sourceTag.clear();
    record.file = file;
    record.line = line;
    record.message = message;
    publishRecord(std::move(record));
}

void Logger::startKernelReader()
{
    if (m_kernelThreadRunning.exchange(true)) {
        return;
    }
    m_kernelThread = std::thread(&Logger::kernelReaderLoop, this);
}

void Logger::stopKernelReader()
{
    if (!m_kernelThreadRunning.exchange(false)) {
        return;
    }
    if (m_kernelFd >= 0) {
        close(m_kernelFd);
        m_kernelFd = -1;
    }
    if (m_kernelThread.joinable()) {
        m_kernelThread.join();
    }
}

void Logger::kernelReaderLoop()
{
    int fd = open("/dev/kmsg", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        m_kernelThreadRunning = false;
        return;
    }
    m_kernelFd = fd;
    lseek(fd, 0, SEEK_END);

    std::string pending;
    while (m_kernelThreadRunning) {
        pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLIN;
        const int pollResult = poll(&pfd, 1, 250);
        if (!m_kernelThreadRunning) {
            break;
        }
        if (pollResult <= 0 || (pfd.revents & POLLIN) == 0) {
            continue;
        }

        char buffer[4096];
        const ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
        if (bytesRead <= 0) {
            continue;
        }

        pending.append(buffer, static_cast<size_t>(bytesRead));
        size_t newline = std::string::npos;
        while ((newline = pending.find('\n')) != std::string::npos) {
            std::string line = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            if (line.empty()) {
                continue;
            }

            int level = 6;
            int facility = 0;
            std::string message;
            parseKernelLine(line, level, facility, message);

            LogRecord record;
            record.type = logTypeFromKernelLevel(level);
            record.source = "kernel";
            record.sourceTag = "kernel:" + std::to_string(facility);
            record.file = "kernel";
            record.line = 0;
            record.message = message;
            publishRecord(std::move(record));
        }
    }

    close(fd);
    if (m_kernelFd == fd) {
        m_kernelFd = -1;
    }
}
