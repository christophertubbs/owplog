//
// Created by christopher.tubbs on 6/3/26.
//
#include "owplog/core/logger.h"

namespace owplog::core {
    namespace {
        Logger& root_logger() {
            static Logger logger;
            return logger;
        }
    }

    void shutdown() {
        root_logger().shutdown();
    }

    void set_level(const LogLevel level) {
        root_logger().set_level(level);
    }

    void set_formatter(const std::string& pattern) {
        root_logger().set_formatter(pattern);
    }

    void set_formatter(const char* pattern) {
        root_logger().set_formatter(pattern);
    }

    void add_handler(const handlers::LogHandlerPtr handler) {
        root_logger().add_handler(handler);
    }

    void clear_handlers() {
        root_logger().clear_handlers();
    }

    LogLevel level() {
        return root_logger().level();
    }

    void add_filter(const LogFilterPtr filter) {
        root_logger().add_filter(filter);
    }

    void clear_filters() {
        root_logger().clear_filters();
    }

    bool has_handlers() {
        return root_logger().has_handlers();
    }

    bool is_configured() {
        return root_logger().is_configured();
    }

    void log(
        const LogLevel level,
        const std::string_view message,
        const std::string_view logger_name,
        const SourceLocation& source,
        const LogOrigin origin,
        const int rank,
        const std::string_view language
    ) {
        root_logger().log(level, message, logger_name, source, origin, rank, language);
    }

    void error(
        const std::string_view message,
        const std::string_view logger_name,
        const SourceLocation& source,
        const LogOrigin origin,
        const int rank,
        const std::string_view language
    ) {
        root_logger().error(message, logger_name, source, origin, rank, language);
    }

    void info(
        const std::string_view message,
        const std::string_view logger_name,
        const SourceLocation& source,
        const LogOrigin origin,
        const int rank,
        const std::string_view language
    ) {
        root_logger().info(message, logger_name, source, origin, rank, language);
    }

    void debug(
        const std::string_view message,
        const std::string_view logger_name,
        const SourceLocation& source,
        const LogOrigin origin,
        const int rank,
        const std::string_view language
    ) {
        root_logger().debug(message, logger_name, source, origin, rank, language);
    }

    void critical(
        const std::string_view message,
        const std::string_view logger_name,
        const SourceLocation& source,
        const LogOrigin origin,
        const int rank,
        const std::string_view language
    ) {
        root_logger().critical(message, logger_name, source, origin, rank, language);
    }

    void flush() {
        root_logger().flush();
    }

    void configure(const LoggerConfigurationArguments& configuration) {
        root_logger().configure(configuration);
    }
}