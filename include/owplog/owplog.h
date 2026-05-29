//
// Created by christopher.tubbs on 5/28/26.
//

#ifndef OWPLOG_OWPLOG_H
#define OWPLOG_OWPLOG_H
#include "handlers/console_handler.h"
#include "handlers/file_handler.h"
#include "formatter.hpp"
#include "filter.hpp"
#include "log_level.hpp"
#include "logger.hpp"
#include <map>
#include "utility.hpp"

namespace owplog {
    using handlers::FileHandler;
    using handlers::ConsoleHandler;
    using owplog::Logger;
    using owplog::LogFilter;
    using owplog::LogLevel;
    using owplog::PatternFormatter;
    using owplog::SubstringFilter;
    using owplog::LevelFilter;;
    using owplog::handlers::LogHandlerPtr;

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
    const LogHandlerPtr create_handler(const std::string_view handler_type, Args... args) {
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
}
#endif //OWPLOG_OWPLOG_H
