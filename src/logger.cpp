#include "owplog/logger.hpp"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace owplog {

namespace {

#ifdef _WIN32
/** Windows wrapper for duplicating file descriptors. */
int OWPLOG_dup(int fd) { return _dup(fd); }
/** Windows wrapper for replacing a file descriptor. */
int OWPLOG_dup2(int old_fd, int new_fd) { return _dup2(old_fd, new_fd); }
/** Windows wrapper for closing file descriptors. */
int owplog_close(int fd) { return _close(fd); }
/** Windows wrapper for reading bytes from file descriptors. */
int OWPLOG_read(int fd, void* buffer, unsigned int count) { return _read(fd, buffer, count); }
/** Windows wrapper for creating anonymous pipes. */
int OWPLOG_pipe(int* fds) { return _pipe(fds, 4096, _O_BINARY); }
/** Windows wrapper for retrieving FILE* descriptor. */
int OWPLOG_fileno(FILE* stream) { return _fileno(stream); }
#else
/** POSIX wrapper for duplicating file descriptors. */
int OWPLOG_dup(int fd) { return dup(fd); }
/** POSIX wrapper for replacing a file descriptor. */
int OWPLOG_dup2(int old_fd, int new_fd) { return dup2(old_fd, new_fd); }
/** POSIX wrapper for closing file descriptors. */
int owplog_close(int fd) { return close(fd); }
/** POSIX wrapper for reading bytes from file descriptors. */
int OWPLOG_read(int fd, void* buffer, unsigned int count) { return static_cast<int>(read(fd, buffer, count)); }
/** POSIX wrapper for creating anonymous pipes. */
int OWPLOG_pipe(int* fds) { return pipe(fds); }
/** POSIX wrapper for retrieving FILE* descriptor. */
int OWPLOG_fileno(FILE* stream) { return fileno(stream); }
#endif

/**
 * @brief Internal helper that redirects stdout/stderr into logger records.
 */
class LoggerStdioCapture {
public:
    /**
     * @brief Construct capture helper.
     * @param logger Logger used for forwarding captured lines.
     */
    explicit LoggerStdioCapture(Logger& logger)
        : logger_(logger),
          running_(false) {}

    /** Ensure capture is stopped during destruction. */
    ~LoggerStdioCapture() {
        stop();
    }

    /**
     * @brief Start capture threads for selected streams.
     * @param capture_stdout Whether stdout should be captured.
     * @param capture_stderr Whether stderr should be captured.
     * @return True on success.
     */
    bool start(bool capture_stdout, bool capture_stderr) {
        if (running_) {
            return false;
        }

        // stdout capture success indicator (or trivially true when disabled).
        const bool stdout_ok = !capture_stdout || start_one(stdout_state_, stdout, LogOrigin::CapturedStdout, "stdout");
        // stderr capture success indicator (or trivially true when disabled).
        const bool stderr_ok = !capture_stderr || start_one(stderr_state_, stderr, LogOrigin::CapturedStderr, "stderr");
        if (!stdout_ok || !stderr_ok) {
            stop();
            return false;
        }

        running_ = true;
        return true;
    }

    /**
     * @brief Stop capture threads and restore original descriptors.
     */
    void stop() {
        if (!running_) {
            return;
        }
        stop_one(stdout_state_);
        stop_one(stderr_state_);
        running_ = false;
    }

    /**
     * @brief Query capture active state.
     * @return True when capture helper is running.
     */
    bool active() const {
        return running_;
    }

private:
    /** Per-stream redirection state. */
    struct StreamState {
        /** True when this stream redirection is active. */
        bool active = false;
        /** Original file descriptor saved before redirection. */
        int original_fd = -1;
        /** Read-side file descriptor for capture pipe. */
        int read_fd = -1;
        /** Target descriptor being redirected (stdout/stderr). */
        int target_fd = -1;
        /** Reader thread consuming redirected bytes. */
        std::thread reader;
        /** Origin metadata attached to captured records. */
        LogOrigin origin = LogOrigin::Application;
        /** Logger name for captured messages. */
        std::string label;
    };

    /**
     * @brief Start redirection for one stream.
     * @param state Mutable stream state slot.
     * @param stream C stdio stream pointer.
     * @param origin Origin metadata for forwarded records.
     * @param label Logger name for forwarded records.
     * @return True on success.
     */
    bool start_one(StreamState& state, FILE* stream, LogOrigin origin, const std::string& label) {
        if (state.active) {
            return false;
        }

        // File descriptor associated with FILE* stream.
        const int target_fd = OWPLOG_fileno(stream);
        if (target_fd < 0) {
            return false;
        }

        std::fflush(stream);
        // Saved copy used to restore the original stream on stop.
        const int original_fd = OWPLOG_dup(target_fd);
        if (original_fd < 0) {
            return false;
        }

        // Pipe descriptors used for stream redirection.
        int pipe_fds[2] = {-1, -1};
        if (OWPLOG_pipe(pipe_fds) != 0) {
            owplog_close(original_fd);
            return false;
        }

        // Redirect target stream to pipe write side.
        if (OWPLOG_dup2(pipe_fds[1], target_fd) < 0) {
            owplog_close(original_fd);
            owplog_close(pipe_fds[0]);
            owplog_close(pipe_fds[1]);
            return false;
        }

        // Parent process only needs read side after descriptor replacement.
        owplog_close(pipe_fds[1]);

        state.active = true;
        state.original_fd = original_fd;
        state.read_fd = pipe_fds[0];
        state.target_fd = target_fd;
        state.origin = origin;
        state.label = label;
        // Reader thread drains pipe and forwards line-delimited records.
        state.reader = std::thread([this, &state]() {
            // Buffered partial line bytes that span read chunks.
            std::string pending;
            // Scratch buffer used for fd reads.
            char buffer[256];
            while (true) {
                // Number of bytes read from pipe.
                const int bytes = OWPLOG_read(state.read_fd, buffer, sizeof(buffer));
                if (bytes <= 0) {
                    break;
                }

                pending.append(buffer, static_cast<std::size_t>(bytes));
                // Scan pending buffer for newline-delimited complete lines.
                std::size_t pos = 0;
                while (true) {
                    // Index of next newline in pending buffer.
                    const std::size_t newline = pending.find('\n', pos);
                    if (newline == std::string::npos) {
                        pending.erase(0, pos);
                        break;
                    }
                    // Completed line text excluding delimiter.
                    const std::string line = pending.substr(pos, newline - pos);
                    if (!line.empty()) {
                        logger_.log(LogLevel::Info, line, state.label, {}, state.origin);
                    }
                    pos = newline + 1;
                }
            }

            // Flush trailing non-newline text as final record.
            if (!pending.empty()) {
                logger_.log(LogLevel::Info, pending, state.label, {}, state.origin);
            }
            owplog_close(state.read_fd);
            state.read_fd = -1;
        });

        return true;
    }

    /**
     * @brief Stop one stream redirection and restore original descriptor.
     * @param state Mutable stream state slot.
     */
    void stop_one(StreamState& state) {
        if (!state.active) {
            return;
        }

        // Flush redirected stream before restoring original descriptor.
        std::fflush(state.target_fd == OWPLOG_fileno(stdout) ? stdout : stderr);
        OWPLOG_dup2(state.original_fd, state.target_fd);
        owplog_close(state.original_fd);
        state.original_fd = -1;

        if (state.reader.joinable()) {
            state.reader.join();
        }
        state.active = false;
    }

    /** Logger used to emit captured lines. */
    Logger& logger_;
    /** Atomic running flag for quick active checks. */
    std::atomic<bool> running_;
    /** State for stdout capture path. */
    StreamState stdout_state_;
    /** State for stderr capture path. */
    StreamState stderr_state_;
};

} // namespace

class Logger::StdioCapture : public LoggerStdioCapture {
public:
    /** @brief Construct logger stdio capture bridge. */
    explicit StdioCapture(Logger& logger)
        : LoggerStdioCapture(logger) {}
};

/** @brief Access global logger singleton instance. */
Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

/** @brief Construct logger with default info level and pattern formatter. */
Logger::Logger()
    : level_(LogLevel::Info),
      formatter_(std::make_shared<PatternFormatter>()),
      running_(false),
      stop_requested_(false),
      in_flight_(0) {}

/** @brief Ensure clean shutdown during static teardown. */
Logger::~Logger() {
    shutdown();
}

/** @brief Start worker thread if not already running. */
void Logger::start() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (running_) {
        return;
    }
    stop_requested_ = false;
    worker_ = std::thread(&Logger::worker_loop, this);
    running_ = true;
}

/** @brief Stop stdio capture and terminate worker thread. */
void Logger::shutdown() {
    stop_stdio_capture();

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (!running_) {
            return;
        }
        stop_requested_ = true;
    }
    queue_cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }

    std::lock_guard<std::mutex> lock(queue_mutex_);
    running_ = false;
}

/** @brief Update process-level threshold. */
void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    level_ = level;
}

/** @brief Read process-level threshold. */
LogLevel Logger::level() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return level_;
}

/** @brief Set active formatter or restore default formatter. */
void Logger::set_formatter(LogFormatterPtr formatter) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    formatter_ = formatter ? std::move(formatter) : std::make_shared<PatternFormatter>();
}

/** @brief Append a sink to dispatch list. */
void Logger::add_sink(LogSinkPtr sink) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    sinks_.push_back(std::move(sink));
}

/** @brief Remove all sinks. */
void Logger::clear_sinks() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    sinks_.clear();
}

/** @brief Append a process-level filter. */
void Logger::add_filter(LogFilterPtr filter) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    filters_.push_back(std::move(filter));
}

/** @brief Remove all process-level filters. */
void Logger::clear_filters() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    filters_.clear();
}

/** @brief Add default console sink when none exists. */
void Logger::configure_default_console() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    if (sinks_.empty()) {
        sinks_.push_back(std::make_shared<ConsoleSink>());
    }
}

/** @brief Add file sink and convert exceptions to boolean status. */
bool Logger::add_file_sink(const std::string& path, bool append) {
    try {
        add_sink(std::make_shared<FileSink>(path, append));
        return true;
    } catch (...) {
        return false;
    }
}

/** @brief Start stdout/stderr capture helper. */
bool Logger::start_stdio_capture(bool capture_stdout, bool capture_stderr) {
    if (!capture_stdout && !capture_stderr) {
        return false;
    }

    if (!stdio_capture_) {
        stdio_capture_ = std::make_unique<StdioCapture>(*this);
    }
    return stdio_capture_->start(capture_stdout, capture_stderr);
}

/** @brief Stop stdout/stderr capture helper. */
void Logger::stop_stdio_capture() {
    if (stdio_capture_) {
        stdio_capture_->stop();
    }
}

/** @brief Query whether stdio capture helper is active. */
bool Logger::stdio_capture_active() const {
    return stdio_capture_ && stdio_capture_->active();
}

/** @brief Query whether at least one sink is configured. */
bool Logger::has_sinks() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return !sinks_.empty();
}

/**
 * @brief Evaluate process-level filters.
 * @param record Record under evaluation.
 * @return True when all filters allow the record.
 */
bool Logger::passes_filters(const LogRecord& record) const {
    for (const auto& filter : filters_) {
        if (filter && !filter->allow(record)) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Enqueue a record for asynchronous processing.
 * @param level Record severity.
 * @param message Message text.
 * @param logger_name Logger name text.
 * @param source Source metadata.
 * @param origin Origin metadata.
 * @param rank Optional rank metadata.
 * @param language Optional caller language/runtime label.
 */
void Logger::log(
    LogLevel level,
    std::string_view message,
    std::string_view logger_name,
    SourceLocation source,
    LogOrigin origin,
    int rank,
    std::string_view language) {
    // Reject disabled sentinel level immediately.
    if (level == LogLevel::Off) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        // Fast threshold check under config lock.
        if (static_cast<int>(level) < static_cast<int>(level_)) {
            return;
        }
    }

    start();

    // Record object populated once and moved into queue.
    LogRecord record;
    record.timestamp = std::chrono::system_clock::now();
    record.level = level;
    record.logger_name = std::string(logger_name);
    record.message = std::string(message);
    record.thread_id = std::this_thread::get_id();
    record.source = std::move(source);
    record.origin = origin;
    record.rank = rank;
    record.language = std::string(language);
    enqueue(std::move(record));
}

/** @brief Push record into queue and signal worker. */
void Logger::enqueue(LogRecord&& record) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.push_back(std::move(record));
        ++in_flight_;
    }
    queue_cv_.notify_one();
}

/** @brief Worker loop that drains queue and dispatches to sinks. */
void Logger::worker_loop() {
    while (true) {
        // Local record storage for current dispatch unit.
        LogRecord record;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this]() {
                return stop_requested_ || !queue_.empty();
            });

            // Stop only when shutdown requested and queue already drained.
            if (queue_.empty()) {
                if (stop_requested_) {
                    break;
                }
                continue;
            }

            // Move one record out of queue for processing outside lock.
            record = std::move(queue_.front());
            queue_.pop_front();
        }

        // Snapshot formatter/sinks/filters so processing uses consistent view.
        LogFormatterPtr formatter;
        std::vector<LogSinkPtr> sinks;
        std::vector<LogFilterPtr> filters;
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            formatter = formatter_;
            sinks = sinks_;
            filters = filters_;
        }

        // Aggregate filter decision.
        bool allowed = true;
        for (const auto& filter : filters) {
            if (filter && !filter->allow(record)) {
                allowed = false;
                break;
            }
        }

        // Format once and fan out to each sink.
        if (allowed && formatter) {
            const std::string formatted = formatter->format(record);
            for (const auto& sink : sinks) {
                if (!sink) {
                    continue;
                }
                // Re-evaluate sink-local filters before write.
                if (!sink->should_log(record)) {
                    continue;
                }
                sink->write(record, formatted);
            }
        }

        // Update in-flight count and notify flush waiters when queue is empty.
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (in_flight_ > 0) {
                --in_flight_;
            }
            if (in_flight_ == 0 && queue_.empty()) {
                flush_cv_.notify_all();
            }
        }
    }
}

/** @brief Wait for queue drain and flush all sink buffers. */
void Logger::flush() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        flush_cv_.wait(lock, [this]() {
            return queue_.empty() && in_flight_ == 0;
        });
    }

    std::vector<LogSinkPtr> sinks;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        sinks = sinks_;
    }
    for (const auto& sink : sinks) {
        if (sink) {
            sink->flush();
        }
    }
}

} // namespace owplog
