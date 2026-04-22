#ifndef OWPLOG_LOG_RECORD_HPP
#define OWPLOG_LOG_RECORD_HPP

/**
 * @file log_record.hpp
 * @brief Log record payload types passed through the async queue.
 */

#include "owplog/log_level.hpp"

#include <chrono>
#include <string>
#include <thread>

namespace owplog {

/**
 * @brief Source category for records written by owplog.
 */
enum class LogOrigin {
    /** Application code called logger APIs directly. */
    Application,
    /** Record originated from captured stdout stream text. */
    CapturedStdout,
    /** Record originated from captured stderr stream text. */
    CapturedStderr,
    /** Record came from C ABI caller. */
    CAbi
};

/**
 * @brief Optional source location metadata for a log call.
 * @note Empty strings / zero line indicate unavailable source data.
 */
struct SourceLocation {
    /** Source filename. */
    std::string file;
    /** Source line number. */
    int line = 0;
    /** Source function name. */
    std::string function;
};

/**
 * @brief Fully populated log message payload consumed by sinks.
 * @note `rank < 0` and empty `language` indicate optional metadata is unset.
 */
struct LogRecord {
    /** Timestamp when the record was created. */
    std::chrono::system_clock::time_point timestamp;
    /** Severity level associated with this record. */
    LogLevel level = LogLevel::Info;
    /** Logical logger name (component/module). */
    std::string logger_name;
    /** Final message content. */
    std::string message;
    /** Producer thread identifier. */
    std::thread::id thread_id;
    /** Optional source location metadata. */
    SourceLocation source;
    /** Origin channel for this record. */
    LogOrigin origin = LogOrigin::Application;
    /** Optional HPC rank (negative when unknown/unset). */
    int rank = -1;
    /** Optional caller language/runtime label (empty when unknown). */
    std::string language;
};

} // namespace owplog

#endif
