#ifndef OWPLOG_CONFIGURATION_HPP
#define OWPLOG_CONFIGURATION_HPP

#include "owplog/log_level.hpp"

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

namespace owplog {

class Logger;

struct ConfigurationMessage {
    std::size_t line_number = 0;
    std::string message;
};

struct ConfigurationResult {
    bool applied = false;
    bool skipped_existing_logger = false;
    std::vector<ConfigurationMessage> warnings;
    std::vector<ConfigurationMessage> errors;

    bool ok() const;
};

struct ConfigurationContext {
    std::string current_working_directory;
    std::string application_name = "owplog";
};

struct FilterConfiguration {
    bool enabled = false;
    bool has_level = false;
    LogLevel level = LogLevel::Info;
    std::string substring;
};

struct ConsoleHandlerConfiguration {
    bool enabled = false;
    bool stderr_for_errors = true;
    bool suppress_captured_streams = true;
    FilterConfiguration filter;
};

struct FileHandlerConfiguration {
    bool enabled = false;
    std::string path;
    bool append = true;
    FilterConfiguration filter;
};

struct LoggerConfiguration {
    bool has_application_name = false;
    std::string application_name;

    bool has_level = false;
    LogLevel level = LogLevel::Info;

    bool has_formatter_pattern = false;
    std::string formatter_pattern;

    FilterConfiguration global_filter;
    ConsoleHandlerConfiguration console_handler;
    FileHandlerConfiguration file_handler;

    bool capture_stdio = false;
    bool capture_stdout = true;
    bool capture_stderr = true;
};

ConfigurationContext default_configuration_context();

LoggerConfiguration parse_logger_configuration(
    std::istream& input,
    ConfigurationResult& result
);

LoggerConfiguration parse_logger_configuration(
    std::istream& input,
    const ConfigurationContext& context,
    ConfigurationResult& result
);

LoggerConfiguration parse_logger_configuration_file(
    const std::string& path,
    ConfigurationResult& result
);

LoggerConfiguration parse_logger_configuration_file(
    const std::string& path,
    const ConfigurationContext& context,
    ConfigurationResult& result
);

ConfigurationResult apply_logger_configuration(
    const LoggerConfiguration& configuration,
    Logger& logger
);

ConfigurationResult configure_global_logger_if_needed(
    const LoggerConfiguration& configuration
);

ConfigurationResult configure_global_logger_from_stream_if_needed(
    std::istream& input
);

ConfigurationResult configure_global_logger_from_stream_if_needed(
    std::istream& input,
    const ConfigurationContext& context
);

ConfigurationResult configure_global_logger_from_file_if_needed(
    const std::string& path
);

ConfigurationResult configure_global_logger_from_file_if_needed(
    const std::string& path,
    const ConfigurationContext& context
);

} // namespace owplog

#endif
