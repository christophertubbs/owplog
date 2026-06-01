#ifndef OWPLOG_LOG_RECORD_HPP
#define OWPLOG_LOG_RECORD_HPP

/**
 * @file log_record.hpp
 * @brief Log record payload types passed through the async queue.
 */

#include "owplog/log_level.hpp"

#include <chrono>
#include <iomanip>
#include <string>
#include <thread>
#include <filesystem>
#include <iostream>

#include "utility.hpp"

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

    inline std::string to_string(LogOrigin origin) {
        switch (origin) {
            case LogOrigin::Application: return "Application";
            case LogOrigin::CapturedStdout: return "CapturedStdout";
            case LogOrigin::CapturedStderr: return "CapturedStderr";
            case LogOrigin::CAbi: return "CAbi";
            default: return "Unknown";
        }
    }

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
        utility::TimePoint monotonic_time = utility::Clock::now();
        pid_t process_id = getpid();
    };

    struct LogFormatRules
    {
        std::string date_format = "%Y-%m-%d %H:%M:%S" ;
    };

    const std::map<std::string_view, std::string> RECORD_FIELD_TO_PLACEHOLDER_MAP = {
        {"rank", "%(rank)d"},
        {"language", "%(language)s"},
        {"level", "%(levelno}d"},
        {"level_name", "%(levelname)s"},
        {"logger_name", "%(name)s"},
        {"message", "%(message)s"},
        {"timestamp", "%(asctime)s"},
        {"thread_id", "%(thread)d"},
        {"origin", "%(origin)s"},
        {"file", "%(filename)s"},
        {"function", "%(funcName)s"},
        {"line", "%(lineno)d"},
        {"relativeCreated", "%(relativeCreated)d"},
        {"pathname", "%(pathname)s"},
        {"elapsed", "%(elapsed)s"},
        {"process", "%(process)d"}
    };

    const std::map<std::string_view, std::function<std::string(const LogRecord&, const LogFormatRules&)>> RECORD_FIELD_TO_VALUE = {
        {
            "rank",
            [](const LogRecord& record, const LogFormatRules& rules) {
                return std::to_string(record.rank);
            }
        },
        {
            "level",
            [](const LogRecord& record, const LogFormatRules& rules) {
                return std::to_string(static_cast<int>(record.level));
            }
        },
        {
            "level_name",
            [](const LogRecord& record, const LogFormatRules& rules) {
                return to_string(record.level);
            }
        },
        {
            "logger_name",
            [](const LogRecord& record, const LogFormatRules& rules) { return record.logger_name; }
        },
        {
            "message",
            [](const LogRecord& record, const LogFormatRules& rules) { return record.message; }
        },
        {
            "timestamp",
            [](const LogRecord& record, const LogFormatRules& rules) {
                const std::time_t time_value = std::chrono::system_clock::to_time_t(record.timestamp);
                std::tm local_time{};

                const std::tm* time_conversion = localtime_r(&time_value, &local_time);

                if (time_conversion == nullptr) {
                    THROW_LOGIC_ERROR("Failed to convert system_clock::time_point to local time");
                }
                std::ostringstream formatted_time_builder;
                formatted_time_builder << std::put_time(&local_time, rules.date_format.c_str());
                return formatted_time_builder.str();
            }
        },
        {
            "thread_id",
            [](const LogRecord& record, const LogFormatRules& rules) {
                std::ostringstream thread_id_builder;
                thread_id_builder << record.thread_id;
                return thread_id_builder.str();
            }
        },
        {
            "origin",
            [](const LogRecord& record, const LogFormatRules& rules) { return to_string(record.origin); }
        },
        {
            "file",
            [](const LogRecord& record, const LogFormatRules& rules) {
                return std::filesystem::path(record.source.file).filename().string();
            }
        },
        {
            "pathname",
            [](const LogRecord& record, const LogFormatRules& rules) {
                return record.source.file;
            }
        },
        {
            "language",
            [](const LogRecord& record, const LogFormatRules& rules) { return record.language; }
        },
        {
            "function",
            [](const LogRecord& record, const LogFormatRules& rules) { return record.source.function; }
        },
        {
            "line",
            [](const LogRecord& record, const LogFormatRules& rules) { return std::to_string(record.source.line); }
        },
        {
            "relativeCreated",
            [](const LogRecord& record, const LogFormatRules& rules) {
                const auto elapsed_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    record.monotonic_time.time_since_epoch() - utility::INITIALIZATION_TIME.time_since_epoch()
                );
                return std::to_string(elapsed_duration.count());
            }
        },
        {
            "elapsed",
            [](const LogRecord& record, const LogFormatRules& rules) {
                std::string duration = utility::format_duration(
                    record.monotonic_time,
                    utility::INITIALIZATION_TIME
                );
                return duration;
            }
        },
        {
            "process",
            [](const LogRecord& record, const LogFormatRules& rules) { return std::to_string(record.process_id); }
        }
    };

    inline std::string populate_record_field(
        const std::string_view& template_string,
        const LogRecord& record,
        const std::string_view& field,
        const LogFormatRules& rules
    ) {
        const std::string& placeholder = RECORD_FIELD_TO_PLACEHOLDER_MAP.at(field);
        const std::string value = RECORD_FIELD_TO_VALUE.at(field)(record, rules);
        std::string result = utility::replace_all_text(template_string, placeholder, value);
        return result;
    }

    inline std::string format_log_record(
        const LogRecord& record,
        const std::string_view& pattern,
        const std::string& date_fmt = "%Y-%m-%d %H:%M:%S"
    ) {
        std::string result{pattern};
        const LogFormatRules rules{
            .date_format = date_fmt
        };
        for (const auto& [field, retriever] : RECORD_FIELD_TO_PLACEHOLDER_MAP) {
            try {
                result = populate_record_field(result, record, field, rules);
            } catch (const std::exception& exc) {
                std::cerr << "Failed to populate field '" << field << "' when formatting a log record in " << __FILE__ << "::" << __func__ << "#" << __LINE__ << ": " << exc.what() << std::endl;
                throw exc;
            }
        }

        return std::move(result);
    }

} // namespace owplog

#endif
