#ifndef OWPLOG_LOG_LEVEL_HPP
#define OWPLOG_LOG_LEVEL_HPP

/**
 * @file log_level.hpp
 * @brief Log level definitions and conversion helpers.
 */

#include <string_view>

namespace owplog {

/**
 * @brief Canonical log levels used throughout owplog.
 */
enum class LogLevel {
    /** Trace-level diagnostic output. */
    Trace = 0,
    /** Debug-level output. */
    Debug = 10,
    /** Informational output. */
    Info = 20,
    /** Warning output. */
    Warning = 30,
    /** Error output. */
    Error = 40,
    /** Critical output. */
    Critical = 50,
    /** Logging disabled. */
    Off = 60
};

/**
 * @brief Convert a log level enum to a display string.
 * @param level Input level.
 * @return Uppercase textual level label.
 * @pre `level` should be a known enumerator.
 * @post Returns a stable string literal with static storage duration.
 * @throws No exceptions.
 */
inline const char* to_string(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
        case LogLevel::Off: return "OFF";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Map an integer value to the nearest owplog log level threshold.
 * @param value Numeric level.
 * @return Mapped @ref LogLevel value.
 * @pre None.
 * @post Always returns a valid @ref LogLevel enumerator.
 * @throws No exceptions.
 */
inline LogLevel from_int(int value) {
    if (value <= static_cast<int>(LogLevel::Trace)) return LogLevel::Trace;
    if (value <= static_cast<int>(LogLevel::Debug)) return LogLevel::Debug;
    if (value <= static_cast<int>(LogLevel::Info)) return LogLevel::Info;
    if (value <= static_cast<int>(LogLevel::Warning)) return LogLevel::Warning;
    if (value <= static_cast<int>(LogLevel::Error)) return LogLevel::Error;
    if (value <= static_cast<int>(LogLevel::Critical)) return LogLevel::Critical;
    return LogLevel::Off;
}

} // namespace owplog

#endif
