#include "owplog/handler.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace owplog {

/**
 * @brief Add sink-local filter.
 * @param filter Filter to append.
 */
void LogHandler::add_filter(LogFilterPtr filter) {
    std::lock_guard lock(this->filter_mutex_);
    this->filters_.push_back(std::move(filter));
}

/**
 * @brief Remove all sink-local filters.
 */
void LogHandler::clear_filters() {
    std::lock_guard lock(this->filter_mutex_);
    this->filters_.clear();
}

/**
 * @brief Evaluate sink-local filters for a record.
 * @param record Record under evaluation.
 * @return True when all local filters allow record.
 */
bool LogHandler::should_log(const LogRecord& record) const {
    std::lock_guard lock(filter_mutex_);
    return std::all_of(
        this->filters_.begin(),
        this->filters_.end(),
        [&record](const auto& filter) {
            return !filter || filter->allow(record);
        }
    );
}

/**
 * @brief Default no-op flush implementation.
 */
void LogHandler::flush() {}

/**
 * @brief Construct console sink behavior.
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
    ::write(target_descriptor, "\n", 1);
}

/**
 * @brief Flush stdout and stderr.
 */
void ConsoleHandler::flush() {
    std::lock_guard lock(this->io_mutex_);
    ::fdatasync(this->stdout_descriptor_);
    ::fdatasync(this->stderr_descriptor_);
}

/**
 * @brief Construct file sink stream.
 * @param path Log output path.
 * @param append Append mode when true, truncate when false.
 */
FileHandler::FileHandler(const std::string& path, bool append) {
    // Stream mode selected from append/truncate behavior.
    const auto mode = append ? (std::ios::out | std::ios::app) : (std::ios::out | std::ios::trunc);
    this->stream_.open(path, mode);
    if (!this->stream_) {
        throw std::runtime_error("Unable to open log file: " + path);
    }
}

/**
 * @brief Write one formatted line to file sink.
 * @param formatted Formatted record text.
 */
void FileHandler::write(const LogRecord&, const std::string& formatted) {
    std::lock_guard lock(this->file_mutex_);
    this->stream_ << formatted << '\n';
}

/**
 * @brief Flush file sink stream.
 */
void FileHandler::flush() {
    std::lock_guard lock(this->file_mutex_);
    this->stream_.flush();
}

} // namespace owplog
