//
// Created by christopher.tubbs on 5/28/26.
//
#include "owplog/handlers/console_handler.h"
namespace owplog::handlers {
    /**
     * @brief Construct console handler behavior.
     * @param stderr_for_errors Route ERROR+ records to stderr.
     * @param suppress_captured_streams Skip output for captured stdio records.
     */
    ConsoleHandler::ConsoleHandler(
        bool stderr_for_errors,
        bool suppress_captured_streams
    ) : stderr_for_errors_(stderr_for_errors), suppress_captured_streams_(suppress_captured_streams) {}

    /**
     * @brief Write formatted record to stdout/stderr.
     * @param record Original record metadata.
     * @param formatted Formatted record text.
     */
    void ConsoleHandler::write(const LogRecord& record, const std::string& formatted) {
        bool origin_is_captured_stream = record.origin == LogOrigin::CapturedStderr;
        origin_is_captured_stream |= record.origin == LogOrigin::CapturedStdout;
        if (this->suppress_captured_streams_ && origin_is_captured_stream) {
            return;
        }

        std::lock_guard lock(io_mutex_);

        const bool is_high_severity = static_cast<int>(record.level) >= static_cast<int>(LogLevel::Error);

        int target_descriptor = -1;
        // Select stderr for high-severity output when configured.
        if (this->stderr_for_errors_ && is_high_severity) {
            target_descriptor = this->stderr_descriptor_;
        } else {
            target_descriptor = this->stdout_descriptor_;
        }

        const char* next_byte = formatted.data();
        std::size_t remaining_byte_count = formatted.size();

        while (remaining_byte_count > 0U) {
            const ssize_t written_byte_count = ::write(
                target_descriptor,
                next_byte,
                remaining_byte_count
            );

            if (written_byte_count < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                throw std::runtime_error("Unable to write to console: " + std::to_string(written_byte_count));
            }

            next_byte += written_byte_count;
            remaining_byte_count -= static_cast<std::size_t>(written_byte_count);
        }

        const ssize_t characters_written = ::write(target_descriptor, "\n", 1);

        if (characters_written != 1) {
            throw std::runtime_error("Unable to write newline to console");
        }
    }

    void ConsoleHandler::set_stderr_descriptor(const int descriptor) {
        this->stderr_descriptor_ = descriptor;
    }

    void ConsoleHandler::set_stdout_descriptor(const int descriptor) {
        this->stdout_descriptor_ = descriptor;
    }
}