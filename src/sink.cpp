#include "owplog/sink.hpp"

#include <iostream>
#include <stdexcept>

namespace owplog {

/**
 * @brief Add sink-local filter.
 * @param filter Filter to append.
 */
void LogSink::add_filter(LogFilterPtr filter) {
    std::lock_guard<std::mutex> lock(filter_mutex_);
    filters_.push_back(std::move(filter));
}

/**
 * @brief Remove all sink-local filters.
 */
void LogSink::clear_filters() {
    std::lock_guard<std::mutex> lock(filter_mutex_);
    filters_.clear();
}

/**
 * @brief Evaluate sink-local filters for a record.
 * @param record Record under evaluation.
 * @return True when all local filters allow record.
 */
bool LogSink::should_log(const LogRecord& record) const {
    std::lock_guard<std::mutex> lock(filter_mutex_);
    for (const auto& filter : filters_) {
        if (filter && !filter->allow(record)) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Default no-op flush implementation.
 */
void LogSink::flush() {}

/**
 * @brief Construct console sink behavior.
 * @param stderr_for_errors Route ERROR+ records to stderr.
 * @param suppress_captured_streams Skip output for captured stdio records.
 */
ConsoleSink::ConsoleSink(bool stderr_for_errors, bool suppress_captured_streams)
    : stderr_for_errors_(stderr_for_errors),
      suppress_captured_streams_(suppress_captured_streams) {}

/**
 * @brief Write formatted record to stdout/stderr.
 * @param record Original record metadata.
 * @param formatted Formatted record text.
 */
void ConsoleSink::write(const LogRecord& record, const std::string& formatted) {
    if (suppress_captured_streams_
        && (record.origin == LogOrigin::CapturedStdout || record.origin == LogOrigin::CapturedStderr)) {
        return;
    }

    std::lock_guard<std::mutex> lock(io_mutex_);
    // Select stderr for high-severity output when configured.
    const bool use_stderr = stderr_for_errors_ && static_cast<int>(record.level) >= static_cast<int>(LogLevel::Error);
    if (use_stderr) {
        std::cerr << formatted << '\n';
    } else {
        std::cout << formatted << '\n';
    }
}

/**
 * @brief Flush stdout and stderr.
 */
void ConsoleSink::flush() {
    std::lock_guard<std::mutex> lock(io_mutex_);
    std::cout.flush();
    std::cerr.flush();
}

/**
 * @brief Construct file sink stream.
 * @param path Log output path.
 * @param append Append mode when true, truncate when false.
 */
FileSink::FileSink(const std::string& path, bool append) {
    // Stream mode selected from append/truncate behavior.
    const auto mode = append ? (std::ios::out | std::ios::app) : (std::ios::out | std::ios::trunc);
    stream_.open(path, mode);
    if (!stream_) {
        throw std::runtime_error("Unable to open log file: " + path);
    }
}

/**
 * @brief Write one formatted line to file sink.
 * @param formatted Formatted record text.
 */
void FileSink::write(const LogRecord&, const std::string& formatted) {
    std::lock_guard<std::mutex> lock(file_mutex_);
    stream_ << formatted << '\n';
}

/**
 * @brief Flush file sink stream.
 */
void FileSink::flush() {
    std::lock_guard<std::mutex> lock(file_mutex_);
    stream_.flush();
}

} // namespace owplog
