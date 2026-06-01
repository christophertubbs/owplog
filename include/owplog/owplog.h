//
// Created by christopher.tubbs on 5/28/26.
//

#ifndef OWPLOG_OWPLOG_H
#define OWPLOG_OWPLOG_H
#include "formatter.hpp"
#include "filter.hpp"
#include "log_level.hpp"
#include <map>
#include "utility.hpp"
#include "owplog/core/core.h"
#include "owplog/handlers/handlers.h"

namespace owplog {
    using FileHandler = handlers::FileHandler;
    using ConsoleHandler = handlers::ConsoleHandler;
    using Logger = core::Logger;
    using LoggerConfigurationArguments = core::LoggerConfigurationArguments;
    using owplog::LogFilter;
    using owplog::LogLevel;
    using owplog::PatternFormatter;
    using owplog::SubstringFilter;
    using owplog::LevelFilter;
    using handlers::LogHandlerPtr;

    const std::map<std::string, LogLevel> LOG_LEVEL_MAP = {
        {"DEBUG", LogLevel::Debug},
        {"INFO", LogLevel::Info},
        {"WARNING", LogLevel::Warning},
        {"ERROR", LogLevel::Error},
        {"CRITICAL", LogLevel::Critical}
    };

    template<typename... Args>
    const LogFilter& create_filter(const std::string_view filter_type, Args... args) {
        if (utility::string_type_endswith(filter_type, "SubstringFilter")) {
            return std::move(SubstringFilter(std::forward<Args>(args)...));
        }

        if (utility::string_type_endswith(filter_type, "LevelFilter")) {
            return std::move(LevelFilter(std::forward<Args>(args)...));
        }

        std::ostringstream error_builder;
        error_builder << "'" << filter_type << "' is not a valid filter type. Valid types are: SubstringFilter, LevelFilter.";
        THROW_INVALID_ARGUMENT(error_builder.str());
    }

    template<typename... Args>
    const LogFormatter& create_formatter(const std::string_view formatter_type, Args... args) {
        if (utility::string_type_endswith(formatter_type, "PatternFormatter")) {
            return std::move(PatternFormatter(std::forward<Args>(args)...));
        }

        std::ostringstream error_builder;
        error_builder << "'" << formatter_type << "' is not a valid formatter type. Valid types are: PatternFormatter.";
        THROW_INVALID_ARGUMENT(error_builder.str());
    }

    template<typename... Args>
    LogHandlerPtr create_handler(const std::string_view handler_type, Args... args) {
        if (utility::string_type_endswith(handler_type, "ConsoleHandler")) {
            THROW_INVALID_ARGUMENT("Cannot route a ConsoleHandler - only one ConsoleHandler may be used at a time");
        }

        if (utility::string_type_endswith(handler_type, "FileHandler")) {
            return std::make_shared<FileHandler>(std::forward<Args>(args)...);
        }

        std::ostringstream error_builder;
        error_builder << "'" << handler_type << "' is not a valid handler type. Valid types are: ConsoleHandler, FileHandler.";
        THROW_INVALID_ARGUMENT(error_builder.str());
    }

    inline void basicConfig(const std::string& filename, const std::string& format) {
        const LoggerConfigurationArguments args = {
            .filename = filename,
            .format = format,
        };
        Logger::configure(args);
    }

    inline void basicConfig(
        const std::string& filename,
        const std::string& format,
        const std::string& datefmt
    ) {
        const LoggerConfigurationArguments args {
            .filename = filename,
            .format = format,
            .datefmt = datefmt,
        };

        Logger::configure(args);
    }

    inline void basicConfig(const LoggerConfigurationArguments& configuration) {
        Logger::configure(configuration);
    }

    inline void basicConfig() {
        basicConfig(LoggerConfigurationArguments{});
    }
}

/** Source location helper macro for call-site metadata injection. */
#define OWPLOG_SOURCE_LOCATION ::owplog::SourceLocation{__FILE__, __LINE__, __func__}

/** Convenience macro for full custom log call with explicit level/name/message. */
#define OWPLOG_LOG(level, logger_name, message) \
::owplog::core::Logger::log((level), (message), (logger_name), OWPLOG_SOURCE_LOCATION)

/** Convenience macro for INFO-level messages on default logger name. */
#define OWPLOG_LOG_INFO_MSG(message) \
::owplog::core::Logger::info((message), "owplog", OWPLOG_SOURCE_LOCATION)

/** Convenience macro for WARNING-level messages on default logger name. */
#define OWPLOG_LOG_WARNING_MSG(message) \
::owplog::core::Logger::warning((message), "owplog", OWPLOG_SOURCE_LOCATION)

/** Convenience macro for ERROR-level messages on default logger name. */
#define OWPLOG_LOG_ERROR_MSG(message) \
::owplog::core::Logger::error((message), "owplog", OWPLOG_SOURCE_LOCATION)
#endif //OWPLOG_OWPLOG_H
