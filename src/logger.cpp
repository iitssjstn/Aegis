#include "logger.h"
#include <windows.h>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace aegis {

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

bool Logger::Init(const std::wstring& logFilePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::filesystem::path p(logFilePath);
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);

    m_stream.open(logFilePath, std::ios::out | std::ios::app);
    m_initialized = m_stream.is_open();
    return m_initialized;
}

std::string Logger::CurrentTimestampIso8601() {
    SYSTEMTIME st;
    GetSystemTime(&st); // UTC, bewust: consistent over sessies/tijdzones
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << st.wYear << "-"
        << std::setw(2) << st.wMonth << "-"
        << std::setw(2) << st.wDay << "T"
        << std::setw(2) << st.wHour << ":"
        << std::setw(2) << st.wMinute << ":"
        << std::setw(2) << st.wSecond << "."
        << std::setw(3) << st.wMilliseconds << "Z";
    return oss.str();
}

const char* Logger::LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::FLAG:  return "FLAG";
        case LogLevel::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

void Logger::Log(LogLevel level, const std::string& module, const std::string& message) {
    LogEvent(level, module, message, "");
}

void Logger::LogEvent(LogLevel level, const std::string& module,
                       const std::string& message, const std::string& extraJsonFields) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return;

    std::ostringstream line;
    line << "{"
         << "\"ts\":\"" << CurrentTimestampIso8601() << "\","
         << "\"level\":\"" << LevelToString(level) << "\","
         << "\"module\":\"" << module << "\","
         << "\"message\":\"" << message << "\"";
    if (!extraJsonFields.empty()) {
        line << "," << extraJsonFields;
    }
    line << "}";

    // std::wofstream i.c.m. ASCII-only JSON hier is bewust simpel gehouden;
    // voor non-ASCII paden/namen later omzetten naar UTF-8 encoding laag.
    std::wstring wline(line.str().begin(), line.str().end());
    m_stream << wline << L"\n";
    m_stream.flush(); // Bewust: bij een crash wil je de laatste events nog hebben
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stream.is_open()) m_stream.close();
    m_initialized = false;
}

} // namespace aegis
