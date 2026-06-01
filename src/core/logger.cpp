#include "owplog/core/logger.h"
#include "owplog/formatter.hpp"
#include "owplog/handlers/log_handler.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <memory>
#include <utility>
#include <thread>

#include <unistd.h>

#include "owplog/handlers/file_handler.h"
#include "owplog/handlers/console_handler.h"
#include "owplog/handlers/router.h"

namespace owplog::core {

    /** @brief Access global logger singleton instance. */
    Logger& Logger::instance() {
        static Logger logger;
        return logger;
    }

    bool Logger::is_configured() {
        std::lock_guard lock(instance().state_mutex_);
        return instance().is_configured_;
    }

    void Logger::configure_internal(const LoggerConfigurationArguments& configuration) {
        std::lock_guard<std::mutex> lock(this->state_mutex_);
        if (is_configured_ && !configuration.force) {
            return;
        }
        if (configuration.level.has_value()) {
            this->level_ = configuration.level.value();
        }

        if (configuration.handlers.has_value() && !configuration.handlers->empty() && configuration.filename.has_value()) {
            THROW_INVALID_ARGUMENT("Cannot specify both handlers and filename");
        } else if (configuration.handlers.has_value() && !configuration.handlers->empty()) {
            this->handlers_.clear();
            for (const auto& handler : *configuration.handlers) {
                this->handlers_.push_back(handler);
            }
        } else if (configuration.filename.has_value()) {
            this->handlers_.clear();
            this->handlers_.push_back(
                std::make_shared<handlers::FileHandler>(configuration.filename.value())
            );
        }

        if (configuration.format.has_value()) {
            this->formatter_ = std::make_shared<PatternFormatter>(configuration.format.value());
        }

        if (configuration.datefmt.has_value()) {
            this->datefmt = configuration.datefmt.value();
        }
    }

    /** @brief Construct logger with default info level and pattern formatter. */
    Logger::Logger(const LoggerConfigurationArguments& configuration)
    {
        if (configuration.format.has_value()) {
            this->formatter_ = std::make_shared<PatternFormatter>(configuration.format.value());
        } else if (!this->is_configured_) {
            this->formatter_ = std::make_shared<PatternFormatter>();
        }

        if (configuration.level.has_value()) {
            this->level_ = configuration.level.value();
        } else {
            this->level_ = LogLevel::Info;
        }

        if (configuration.handlers.has_value() && !configuration.handlers->empty() && configuration.filename.has_value()) {
            THROW_INVALID_ARGUMENT("Cannot specify both handlers and filename");
        }

        if (configuration.filename.has_value()) {
            this->handlers_.push_back(
                std::make_shared<handlers::FileHandler>(configuration.filename.value())
            );
        } else if (configuration.handlers.has_value() && !configuration.handlers->empty()) {
            for (const auto& handler : *configuration.handlers) {
                this->handlers_.push_back(handler);
            }
        } else {
            this->handlers_.push_back(
                std::make_shared<handlers::ConsoleHandler>()
            );
        }

        this->internal_start();
    }

    /** @brief Ensure clean shutdown during static teardown. */
    Logger::~Logger() {
        this->internal_shutdown();
    }

    /** @brief Start worker thread if not already running. */
    void Logger::internal_start() {
        std::lock_guard<std::mutex> lock(this->state_mutex_);
        if (this->running_) {
            return;
        }
        this->stop_requested_ = false;
        this->worker_ = std::thread(&Logger::worker_loop, this);
        this->running_ = true;
    }

    void Logger::shutdown() {
        instance().internal_shutdown();
    }

    /** @brief Stop stdio capture and terminate worker thread. */
    void Logger::internal_shutdown() {
        {
            std::lock_guard<std::mutex> lock(this->state_mutex_);
            this->stop_stdio_capture();
            this->stop_requested_ = true;
            if (!this->running_) {
                return;
            }

            this->queue_.request_shutdown();
        }

        if (this->worker_.joinable()) {
            this->worker_.join();
        }
        this->running_ = false;

        this->flush_internal();
    }

    /** @brief Update process-level threshold. */
    void Logger::set_level(LogLevel level) {
        std::lock_guard<std::mutex> lock(instance().state_mutex_);
        instance().level_ = level;
    }

    /** @brief Read process-level threshold. */
    LogLevel Logger::level() {
        std::lock_guard<std::mutex> lock(instance().state_mutex_);
        return instance().level_;
    }

    /** @brief Set active formatter or restore default formatter. */
    void Logger::set_formatter(LogFormatterPtr formatter) {
        std::lock_guard<std::mutex> lock(instance().state_mutex_);
        instance().formatter_ = formatter ? std::move(formatter) : std::make_shared<PatternFormatter>();
    }

    void Logger::set_formatter(const std::string& pattern) {
        std::lock_guard lock(instance().state_mutex_);
        instance().formatter_ = std::make_shared<PatternFormatter>(pattern);
    }

    void Logger::set_formatter(const char* pattern) {
        std::lock_guard lock(instance().state_mutex_);
        instance().formatter_ = std::make_shared<PatternFormatter>(std::string(pattern));
    }

    /** @brief Append a sink to dispatch list. */
    void Logger::add_handler(handlers::LogHandlerPtr handler) {
        std::lock_guard<std::mutex> lock(instance().state_mutex_);
        instance().handlers_.push_back(std::move(handler));
    }

    /** @brief Remove all sinks. */
    void Logger::clear_handlers() {
        std::lock_guard<std::mutex> lock(instance().state_mutex_);
        instance().handlers_.clear();
    }

    /** @brief Append a process-level filter. */
    void Logger::add_filter(LogFilterPtr filter) {
        std::lock_guard<std::mutex> lock(instance().state_mutex_);
        instance().filters_.push_back(std::move(filter));
    }

    /** @brief Remove all process-level filters. */
    void Logger::clear_filters() {
        std::lock_guard<std::mutex> lock(instance().state_mutex_);
        instance().filters_.clear();
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
    void Logger::stop_stdio_capture() const {
        if (this->stdio_capture_) {
            this->stdio_capture_->stop();
        }
    }

    /** @brief Query whether stdio capture helper is active. */
    bool Logger::stdio_capture_active() {
        return this->stdio_capture_ && this->stdio_capture_->active();
    }

    /** @brief Query whether at least one sink is configured. */
    bool Logger::has_handlers() {
        std::lock_guard<std::mutex> lock(instance().state_mutex_);
        return !instance().handlers_.empty();
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
    void Logger::log_message(
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
            std::lock_guard<std::mutex> lock(this->state_mutex_);
            // Fast threshold check under config lock.
            if (static_cast<int>(level) < static_cast<int>(this->level_)) {
                return;
            }
        }

        this->internal_start();

        if (!this->running_ || this->stop_requested_) {
            return;
        }

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

        this->enqueue(std::move(record));
    }

    void Logger::log(
        LogLevel level,
        std::string_view message,
        std::string_view logger_name,
        SourceLocation source,
        LogOrigin origin,
        int rank,
        std::string_view language
    ) {
        instance().log_message(level, message, logger_name, std::move(source), origin, rank, language);
    }

    /** @brief Push record into queue and signal worker. */
    void Logger::enqueue(LogRecord&& record) {
        this->queue_.submit(record);
    }

    /** @brief Worker loop that drains queue and dispatches to sinks. */
    void Logger::worker_loop() {
        while (true) {
            // Local record storage for current dispatch unit.
            std::optional<utility::TrackedDispatch<LogRecord>> next_value = this->queue_.listen_for_submission();

            if (!next_value.has_value()) {
                if (this->queue_.accepting_submissions() && !this->stop_requested_) {
                    continue;
                }
                break;
            }

            // Move one record out of queue for processing outside lock.
            LogRecord& record = next_value->get();

            // Snapshot formatter/sinks/filters so processing uses consistent view.
            LogFormatterPtr formatter;
            std::vector<handlers::LogHandlerPtr> sinks;
            std::vector<LogFilterPtr> filters;
            {
                std::lock_guard<std::mutex> lock(this->state_mutex_);
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
        }
    }

    void Logger::flush() {
        instance().flush_internal();
    }

    /** @brief Wait for queue drain and flush all sink buffers. */
    void Logger::flush_internal() {
        std::lock_guard<std::mutex> lock(this->state_mutex_);
        const std::vector<handlers::LogHandlerPtr> handlers = this->handlers_;

        for (const auto& sink : handlers) {
            if (sink) {
                sink->flush();
            }
        }
    }

} // namespace owplog
