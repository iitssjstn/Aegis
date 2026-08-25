#pragma once
#include <string>
#include <mutex>
#include <fstream>

namespace aegis {

// Ernst-niveau van een logregel. INFO = normale event, FLAG = verdacht
// gedrag dat later door een mens gereviewd moet worden.
enum class LogLevel {
    INFO,
    WARN,
    FLAG,   // Iets dat een menselijke reviewer moet zien na het tournament
    ERROR
};

// Simpele thread-safe logger die JSON-lines wegschrijft naar een lokaal
// logbestand. Bewust GEEN netwerkcomponent hier: reporting naar een
// backend is een aparte, latere stap (zie project-notities).
class Logger {
public:
    static Logger& Instance();

    // Moet één keer bij startup aangeroepen worden met het pad naar de
    // sessie-logfile (bijv. logs/session_<timestamp>.jsonl).
    bool Init(const std::wstring& logFilePath);

    void Log(LogLevel level, const std::string& module, const std::string& message);

    // Voor events met extra structured data (bijv. hashes, PID's).
    // extraJsonFields moet een geldige JSON-fragment string zijn zonder
    // omliggende accolades, bijv.: "\"pid\":1234,\"path\":\"C:\\\\...\""
    void LogEvent(LogLevel level, const std::string& module,
                  const std::string& message, const std::string& extraJsonFields);

    void Shutdown();

private:
    Logger() = default;
    std::mutex m_mutex;
    std::wofstream m_stream;
    bool m_initialized = false;

    static const char* LevelToString(LogLevel level);
    static std::string CurrentTimestampIso8601();
};

} // namespace aegis
