#include "owplog/logger.hpp"
#include "owplog/formatter.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <utility>

#include <unistd.h>
#include <iostream>

namespace owplog {

namespace {

/** POSIX wrapper for duplicating file descriptors. */
int duplicate_file_descriptor(int file_descriptor) { return dup(file_descriptor); }
/** POSIX wrapper for replacing a file descriptor. */
int redirect_file_descriptor(int from, int to) { return dup2(from, to); }
/** POSIX wrapper for closing file descriptors. */
int close_file_descriptor(int file_descriptor) { return close(file_descriptor); }
/** POSIX wrapper for reading bytes from file descriptors. */
int read_descriptor_into_buffer(int file_descriptor, void* buffer, unsigned int count) { return static_cast<int>(read(file_descriptor, buffer, count)); }
/** POSIX wrapper for creating anonymous pipes. */
int create_pipe(int* fds) { return pipe(fds); }
/** POSIX wrapper for retrieving FILE* descriptor. */
int get_stream_file_descriptor(FILE* stream) { return fileno(stream); }

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
        int stream_fd = -1;
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
        const int target_fd = get_stream_file_descriptor(stream);
        if (target_fd < 0) {
            return false;
        }

        std::fflush(stream);
        // Saved copy used to restore the original stream on stop.
        const int original_fd = duplicate_file_descriptor(target_fd);
        if (original_fd < 0) {
            return false;
        }

        // Pipe descriptors used for stream redirection.
        int pipe_fds[2] = {-1, -1};
        int pipe_created = create_pipe(pipe_fds);
        if (pipe_created != 0) {
            fprintf(stderr, "Pipes couldn't be created. Closing the original file descriptor: %d\n", original_fd);
            close_file_descriptor(original_fd);
            return false;
        }

        int second_pipe_redirected = redirect_file_descriptor(pipe_fds[1], target_fd);
        // at this point nothing is really touching the given stream. If it's stderr, whoopsies, no more communication
        if (second_pipe_redirected < 0) {
            // If you're here, it is because something broke
            close_file_descriptor(original_fd);
            close_file_descriptor(pipe_fds[0]);
            close_file_descriptor(pipe_fds[1]);
            return false;
        }

        // Parent process only needs read side after descriptor replacement.
        close_file_descriptor(pipe_fds[1]);

        state.active = true;
        state.stream_fd = original_fd;
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
            size_t buffer_size = sizeof(buffer);
            while (true) {
                // Number of bytes read from pipe.
                const int bytes = read_descriptor_into_buffer(state.read_fd, buffer, buffer_size);
                if (bytes <= 0) {
                    break;
                }

                write(state.stream_fd, buffer, static_cast<std::size_t>(bytes));
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
                        this->logger_.log(LogLevel::Info, line, state.label, {}, state.origin);
                    }
                    pos = newline + 1;
                }
            }

            // Flush trailing non-newline text as final record.
            if (!pending.empty()) {
                LogLevel stateLevel = state.origin == LogOrigin::CapturedStdout ? LogLevel::Info : LogLevel::Error;
                this->logger_.log(stateLevel, pending, state.label, {}, state.origin);
            }
            close_file_descriptor(state.read_fd);
            state.read_fd = -1;
        });

        return true;
    }

    /**
     * @brief Stop one stream redirection and restore original descriptor.
     * @param state Mutable stream state slot.
     */
    static void stop_one(StreamState& state) {
        if (!state.active) {
            return;
        }

        // Flush redirected stream before restoring original descriptor.
        std::fflush(state.target_fd == get_stream_file_descriptor(stdout) ? stdout : stderr);
        redirect_file_descriptor(state.stream_fd, state.target_fd);

        if (state.reader.joinable()) {
            state.reader.join();
        }

        close_file_descriptor(state.stream_fd);
        state.stream_fd = -1;
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
    this->shutdown();
}

/** @brief Start worker thread if not already running. */
void Logger::start() {
    std::lock_guard<std::mutex> lock(this->queue_mutex_);
    if (this->running_) {
        return;
    }
    this->stop_requested_ = false;
    this->worker_ = std::thread(&Logger::worker_loop, this);
    this->running_ = true;
}

/** @brief Stop stdio capture and terminate worker thread. */
void Logger::shutdown() {
    this->stop_stdio_capture();

    {
        std::lock_guard<std::mutex> lock(this->queue_mutex_);
        if (!this->running_) {
            return;
        }
        this->stop_requested_ = true;
    }
    this->queue_signal_.notify_all();
    if (this->worker_.joinable()) {
        this->worker_.join();
    }

    std::lock_guard<std::mutex> lock(this->queue_mutex_);
    this->running_ = false;
}

/** @brief Update process-level threshold. */
void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(this->config_mutex_);
    this->level_ = level;
}

/** @brief Read process-level threshold. */
LogLevel Logger::level() const {
    std::lock_guard<std::mutex> lock(this->config_mutex_);
    return this->level_;
}

/** @brief Set active formatter or restore default formatter. */
void Logger::set_formatter(LogFormatterPtr formatter) {
    std::lock_guard<std::mutex> lock(this->config_mutex_);
    this->formatter_ = formatter ? std::move(formatter) : std::make_shared<PatternFormatter>();
}

void Logger::set_formatter(std::string pattern) {
    std::lock_guard lock(this->config_mutex_);
    this->formatter_ = std::make_shared<PatternFormatter>(pattern);
}

/** @brief Append a sink to dispatch list. */
void Logger::add_handler(LogHandlerPtr handler) {
    std::lock_guard<std::mutex> lock(this->config_mutex_);
    this->handlers_.push_back(std::move(handler));
}

/** @brief Remove all sinks. */
void Logger::clear_handlers() {
    std::lock_guard<std::mutex> lock(this->config_mutex_);
    this->handlers_.clear();
}

/** @brief Append a process-level filter. */
void Logger::add_filter(LogFilterPtr filter) {
    std::lock_guard<std::mutex> lock(this->config_mutex_);
    this->filters_.push_back(std::move(filter));
}

/** @brief Remove all process-level filters. */
void Logger::clear_filters() {
    std::lock_guard<std::mutex> lock(this->config_mutex_);
    this->filters_.clear();
}

/** @brief Add default console sink when none exists. */
void Logger::configure_default_console() {
    std::lock_guard<std::mutex> lock(this->config_mutex_);
    if (this->handlers_.empty()) {
        this->handlers_.push_back(std::make_shared<ConsoleHandler>());
    }
}

/** @brief Add file sink and convert exceptions to boolean status. */
bool Logger::add_file_sink(const std::string& path, bool append) {
    try {
        add_handler(std::make_shared<FileHandler>(path, append));
        return true;
    } catch (...) {
        return false;
    }
}

/** @brief Start stdout/stderr capture helper. */
bool Logger::capture_stdio(bool capture_stdout, bool capture_stderr) {
    if (!capture_stdout && !capture_stderr) {
        return false;
    }

    if (!this->stdio_capture_) {
        this->stdio_capture_ = std::make_unique<StdioCapture>(*this);
    }
    return this->stdio_capture_->start(capture_stdout, capture_stderr);
}

/** @brief Stop stdout/stderr capture helper. */
void Logger::stop_stdio_capture() {
    if (this->stdio_capture_) {
        this->stdio_capture_->stop();
    }
}

/** @brief Query whether stdio capture helper is active. */
bool Logger::stdio_capture_active() const {
    return this->stdio_capture_ && this->stdio_capture_->active();
}

/** @brief Query whether at least one sink is configured. */
bool Logger::has_handlers() const {
    std::lock_guard<std::mutex> lock(this->config_mutex_);
    return !this->handlers_.empty();
}

/**
 * @brief Evaluate process-level filters.
 * @param record Record under evaluation.
 * @return True when all filters allow the record.
 */
bool Logger::passes_filters(const LogRecord& record) const {
    return std::all_of(
        this->filters_.begin(),
        this->filters_.end(),
        [&record](const LogFilterPtr& filter) {
            return filter && filter->allow(record);
        }
    );
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
    std::string_view language
) {
    // Reject disabled sentinel level immediately.
    if (level == LogLevel::Off) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(this->config_mutex_);
        // Fast threshold check under config lock.
        if (static_cast<int>(level) < static_cast<int>(this->level_)) {
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
        std::lock_guard<std::mutex> lock(this->queue_mutex_);
        this->queue_.push_back(std::move(record));
        ++this->in_flight_;
    }
    this->queue_signal_.notify_one();
}

/** @brief Worker loop that drains queue and dispatches to sinks. */
void Logger::worker_loop() {
    while (true) {
        // Local record storage for current dispatch unit.
        LogRecord record;
        {
            std::unique_lock<std::mutex> lock(this->queue_mutex_);
            this->queue_signal_.wait(lock, [this]() {
                return this->stop_requested_ || !this->queue_.empty();
            });

            // Stop only when shutdown requested and queue already drained.
            if (this->queue_.empty()) {
                if (this->stop_requested_) {
                    break;
                }
                continue;
            }

            // Move one record out of queue for processing outside lock.
            record = std::move(this->queue_.front());
            this->queue_.pop_front();
        }

        // Snapshot formatter/sinks/filters so processing uses consistent view.
        LogFormatterPtr formatter;
        std::vector<LogHandlerPtr> sinks;
        std::vector<LogFilterPtr> filters;
        {
            std::lock_guard<std::mutex> lock(this->config_mutex_);
            formatter = this->formatter_;
            sinks = this->handlers_;
            filters = this->filters_;
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
            std::lock_guard<std::mutex> lock(this->queue_mutex_);
            if (this->in_flight_ > 0) {
                --this->in_flight_;
            }
            if (this->in_flight_ == 0 && this->queue_.empty()) {
                this->flush_signal_.notify_all();
            }
        }
    }
}

/** @brief Wait for queue drain and flush all sink buffers. */
void Logger::flush() {
    {
        std::unique_lock<std::mutex> lock(this->queue_mutex_);
        this->flush_signal_.wait(lock, [this]() {
            return this->queue_.empty() && this->in_flight_ == 0;
        });
    }

    std::vector<LogHandlerPtr> handlers;
    {
        std::lock_guard<std::mutex> lock(this->config_mutex_);
        handlers = this->handlers_;
    }

    for (const auto& sink : handlers) {
        if (sink) {
            sink->flush();
        }
    }
}

} // namespace owplog
