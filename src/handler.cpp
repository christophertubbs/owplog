#include "owplog/handler.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <filesystem>

namespace owplog {

/**
 * @brief Add handler-local filter.
 * @param filter Filter to append.
 */
void LogHandler::add_filter(LogFilterPtr filter) {
    this->filters_.push_back(std::move(filter));
}

    /**
     * @brief Remove all handler-local filters.
     */
    void LogHandler::clear_filters() {
        this->filters_.clear();
    }

    void LogHandler::set_error_policy(ErrorPolicy policy) {
        this->error_policy_.set(policy);
    }

    /**
     * @brief Evaluate handler-local filters for a record.
     * @param record Record under evaluation.
     * @return True when all local filters allow record.
     */
    bool LogHandler::should_log(const LogRecord& record) const {
        return this->is_enabled() && this->filters_.all_of(
            [&record](const auto& filter) {
                return !filter || filter->allow(record);
            }
        );
    }

    void LogHandler::enable() {
        this->enabled_ = true;
    }

    void LogHandler::disable() {
        this->enabled_ = false;
    }

    void LogHandler::handle_error(const std::string &message) {
        switch (this->error_policy_.read_copy()) {
            case ErrorPolicy::Count:
                ++this->error_count_;
                if (this->error_count_ >= this->error_limit_) {
                    throw std::runtime_error(message);
                }
                break;
            case ErrorPolicy::Throw:
                throw std::runtime_error(message);
            case ErrorPolicy::Disable:
                this->disable();
                break;

            case ErrorPolicy::Ignore:
            default:
                break;
        }
    }

    void LogHandler::set_error_limit(std::size_t limit) {
        this->error_limit_ = limit;
    }

    void LogHandler::reset_error_count() {
        this->error_count_ = 0;
    }

    bool LogHandler::is_enabled() const {
        return this->enabled_;
    }

    /**
 * @brief Default no-op flush implementation.
 */
void LogHandler::flush() {}

void LogHandler::set_maximum_buffer_size(const std::size_t size) {
    if (size == 0) {
        throw std::invalid_argument("Maximum buffer size cannot be zero");
    }
    this->maximum_buffer_size_ = size;
}

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

/**
 * @brief Flush stdout and stderr.
 */
void ConsoleHandler::flush() {}

    /**
     * @brief Construct file handler stream.
     * @param path Log output path.
     * @param append Append mode when true, truncate when false.
     */
    FileHandler::FileHandler(const std::filesystem::path& path, bool append) {
        // Stream mode selected from append/truncate behavior.
        const auto mode = append ? (std::ios::out | std::ios::app) : (std::ios::out | std::ios::trunc);
        this->stream_.open(path, mode);
        if (!this->stream_) {
            throw std::runtime_error(std::string("Unable to open log file: ") + path.string());
        }
    }

/**
 * @brief Write one formatted line to file handler.
 * @param formatted Formatted record text.
 */
void FileHandler::write(const LogRecord&, const std::string& formatted) {
    std::size_t buffer_size;
    {
        std::lock_guard lock(this->file_mutex_);
        this->buffer_.append(formatted).append("\n");
        buffer_size = this->buffer_.size();
    }
    if (buffer_size >= this->maximum_buffer_size_) {
        this->flush();
    }
}

/**
 * @brief Flush file handler stream.
 */
void FileHandler::flush() {
    std::lock_guard lock(this->file_mutex_);
    this->stream_ << this->buffer_;
    this->buffer_.clear();
    this->stream_.flush();
}

} // namespace owplog
