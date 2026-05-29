#ifndef OWPLOG_LOGGER_HPP
#define OWPLOG_LOGGER_HPP

/**
 * @file logger.hpp
 * @brief Singleton asynchronous logger public interface.
 */

#include "owplog/filter.hpp"
#include "owplog/formatter.hpp"
#include "owplog/handlers/log_handler.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>
#include <filesystem>
#include <atomic>

namespace owplog {

/**
 * @brief Process-wide asynchronous logger singleton.
 *
 * Producers enqueue @ref LogRecord objects and return quickly.
 * A dedicated worker thread dequeues records, applies filters/formatter,
 * and dispatches output to configured sinks.
 *
 * @pre None. The singleton is lazy-initialized on first access.
 * @post Logger lifetime spans the process.
 * @note Most methods are thread-safe unless otherwise documented.
 * @note API methods do not intentionally throw for normal operation.
 *       Allocation failures (for example `std::bad_alloc`) can still propagate.
 */
class Logger final {
    public:
        /**
         * @brief Access singleton instance.
         * @return Reference to global logger.
         * @pre None.
         * @post Returns the same instance for all callers in the process.
         * @throws May propagate static initialization or allocation failures.
         */
        static Logger& instance();

        /**
         * @brief Start worker thread if not already running.
         * @pre None.
         * @post Worker thread is running unless thread creation failed.
         * @throws May propagate thread creation/system errors.
         */
        void start();
        /**
         * @brief Stop worker thread and optional stdio capture.
         * @pre None.
         * @post Worker thread is stopped and stdio capture is inactive.
         * @note Safe to call multiple times.
         */
        void shutdown();

        /**
         * @brief Set process-level minimum accepted level.
         * @param level Minimum severity threshold.
         * @pre `level` is a valid @ref LogLevel value.
         * @post Future records below `level` are dropped before queueing.
         * @throws No intentional exceptions.
         */
        void set_level(LogLevel level);
        /**
         * @brief Read current process-level threshold.
         * @return Configured level threshold.
         * @pre None.
         * @post Logger state is unchanged.
         * @throws No intentional exceptions.
         */
        LogLevel level() const;

        /**
         * @brief Replace active formatter.
         * @param formatter Formatter instance (null restores default pattern formatter).
         * @pre None.
         * @post Formatter used by subsequent records is updated.
         * @throws May propagate allocation failures when restoring default formatter.
         */
        void set_formatter(LogFormatterPtr formatter);

        /**
         * @brief Replace active formatter with a basic string formatter
         * @param pattern The pattern string for a string pattern formatter
         */
        void set_formatter(const std::string& pattern);
        void set_formatter(const char* pattern);
        /**
         * @brief Add a sink destination.
         * @param handler Handler instance to append.
         * @pre `sink` should be non-null for useful output.
         * @post Handler list contains appended sink pointer.
         * @throws May propagate allocation failures when growing sink storage.
         */
        void add_handler(handlers::LogHandlerPtr handler);
        /**
         * @brief Remove all configured sinks.
         * @pre None.
         * @post Handler list is empty.
         * @throws No intentional exceptions.
         */
        void clear_handlers();

        /**
         * @brief Add a process-level filter.
         * @param filter Filter to append.
         * @pre `filter` should be non-null for useful filtering.
         * @post Filter list contains appended filter pointer.
         * @throws May propagate allocation failures when growing filter storage.
         */
        void add_filter(LogFilterPtr filter);
        /**
         * @brief Remove all process-level filters.
         * @pre None.
         * @post Process-level filter list is empty.
         * @throws No intentional exceptions.
         */
        void clear_filters();

        /**
         * @brief Start capturing process stdout/stderr streams.
         * @param capture_stdout Capture stdout when true.
         * @param capture_stderr Capture stderr when true.
         * @return True on success.
         * @pre At least one of `capture_stdout` or `capture_stderr` must be true.
         * @post On success, selected streams are redirected and capture threads are active.
         * @post On failure, capture is not active for newly requested streams.
         * @throws May propagate allocation failures when creating capture infrastructure.
         */
        bool capture_stdio(bool capture_stdout = true, bool capture_stderr = true);
        /**
         * @brief Stop stream capture threads and restore descriptors.
         * @pre None.
         * @post Capture is inactive and descriptors are restored for streams that were captured.
         * @note Safe to call multiple times.
         */
        void stop_stdio_capture();
        /**
         * @brief Check whether stdio capture is active.
         * @return True when capture helper is active.
         * @pre None.
         * @post Logger state is unchanged.
         * @throws No intentional exceptions.
         */
        bool stdio_capture_active() const;
        /**
         * @brief Check whether the logger currently has configured sinks.
         * @return True when at least one handler is configured.
         * @pre None.
         * @post Logger state is unchanged.
         * @throws No intentional exceptions.
         */
        bool has_handlers() const;

        /**
         * @brief Enqueue one log record.
         * @param level Severity level.
         * @param message Message text.
         * @param logger_name Logical logger name.
         * @param source Optional source location.
         * @param origin Record origin category.
         * @param rank Optional HPC rank (-1 when unknown).
         * @param language Optional caller language/runtime label.
         * @pre `level` should not be @ref LogLevel::Off to produce output.
         * @post Accepted records are queued for asynchronous processing.
         * @post Rejected records (threshold/off) leave queue unchanged.
         * @throws May propagate allocation failures while creating queue records.
         */
        void log(
            LogLevel level,
            std::string_view message,
            std::string_view logger_name = "owplog",
            SourceLocation source = {},
            LogOrigin origin = LogOrigin::Application,
            int rank = -1,
            std::string_view language = "C++"
        );

        void error(
            const std::string_view message,
            const std::string_view logger_name = "owplog",
            SourceLocation source = {},
            const LogOrigin origin = LogOrigin::Application,
            const int rank = -1,
            const std::string_view language = "C++"
        ) {
            log(LogLevel::Error, message, logger_name, std::move(source), origin, rank, language);
        }

        void warning(
            const std::string_view message,
            const std::string_view logger_name = "owplog",
            SourceLocation source = {},
            const LogOrigin origin = LogOrigin::Application,
            const int rank = -1,
            const std::string_view language = "C++"
        ) {
            log(LogLevel::Warning, message, logger_name, std::move(source), origin, rank, language);
        }

        void info(
            const std::string_view message,
            const std::string_view logger_name = "owplog",
            SourceLocation source = {},
            const LogOrigin origin = LogOrigin::Application,
            const int rank = -1,
            const std::string_view language = "C++"
        ) {
            log(LogLevel::Info, message, logger_name, std::move(source), origin, rank, language);
        }

        void debug(
            const std::string_view message,
            const std::string_view logger_name = "owplog",
            SourceLocation source = {},
            const LogOrigin origin = LogOrigin::Application,
            const int rank = -1,
            const std::string_view language = "C++"
        ) {
            log(LogLevel::Debug, message, logger_name, std::move(source), origin, rank, language);
        }

        void critical(
            const std::string_view message,
            const std::string_view logger_name = "owplog",
            SourceLocation source = {},
            const LogOrigin origin = LogOrigin::Application,
            const int rank = -1,
            const std::string_view language = "C++"
        ) {
            log(LogLevel::Critical, message, logger_name, std::move(source), origin, rank, language);
        }

        /**
         * @brief Wait until queue drains and flush all sinks.
         * @pre None.
         * @post All records accepted before call completion are dispatched.
         * @post Each configured handler has had `flush()` invoked once.
         * @throws No intentional exceptions.
         */
        void flush() const;

        /** Deleted copy constructor for singleton semantics. */
        Logger(const Logger&) = delete;
        /** Deleted copy assignment for singleton semantics. */
        Logger& operator=(const Logger&) = delete;


    private:
        /** @brief Construct singleton with default formatter/level state. */
        Logger();
        /** @brief Ensure graceful shutdown on process teardown. */
        ~Logger();

        /** @brief Worker thread main loop. */
        void worker_loop();
        /**
         * @brief Evaluate process-level filters.
         * @param record Record under evaluation.
         * @return True when allowed by all filters.
         */
        bool passes_filters(const LogRecord& record) const;
        /**
         * @brief Push record into queue and notify worker.
         * @param record Record to enqueue.
         */
        void enqueue(LogRecord&& record);

        /** Forward declaration for internal stdio capture bridge. */
        class StdioCapture;

        /** Mutex protecting configuration state (level/filters/sinks/formatter). */
        mutable std::mutex state_mutex_;
        /** Global minimum severity threshold. */
        LogLevel level_;
        /** Active formatter instance. */
        LogFormatterPtr formatter_;
        /** Active handler list. */
        std::vector<handlers::LogHandlerPtr> handlers_;
        /** Process-level filter list. */
        std::vector<LogFilterPtr> filters_;

        /** Pending queue consumed by worker thread. */
        utility::TrackedDispatchQueue<LogRecord> queue_;
        /** Dedicated asynchronous worker thread. */
        std::thread worker_;
        /** The maximum number of bytes that may be written prior to flushing */
        uint32_t maximum_buffer_size_ = 1024 * 1024;
        /** True when worker thread is running. */
        std::atomic<bool> running_;
        /** Shutdown request flag observed by worker loop. */
        std::atomic<bool> stop_requested_;

        /** Optional stdio capture subsystem instance. */
        std::unique_ptr<StdioCapture> stdio_capture_;
    };


    /**
     * @brief Enqueue one log record.
     * @param level Severity level.
     * @param message Message text.
     * @param logger_name Logical logger name.
     * @param source Optional source location.
     * @param origin Record origin category.
     * @param rank Optional HPC rank (-1 when unknown).
     * @param language Optional caller language/runtime label.
     * @pre `level` should not be @ref LogLevel::Off to produce output.
     * @post Accepted records are queued for asynchronous processing.
     * @post Rejected records (threshold/off) leave queue unchanged.
     * @throws May propagate allocation failures while creating queue records.
     */
    inline void log(
        const LogLevel level,
        const std::string_view message,
        const std::string_view logger_name = "root",
        SourceLocation source = {},
        const LogOrigin origin = LogOrigin::Application,
        const int rank = -1,
        const std::string_view language = "C++"
    ) {
        Logger::instance().log(level, message, logger_name, std::move(source), origin, rank, language);
    };

    inline void error(
        const std::string_view message,
        const std::string_view logger_name = "root",
        SourceLocation source = {},
        const LogOrigin origin = LogOrigin::Application,
        const int rank = -1,
        const std::string_view language = "C++"
    ) {
        Logger::instance().error(
            message,
            logger_name,
            std::move(source),
            origin,
            rank,
            language
        );
    }

    inline void warning(
        const std::string_view message,
        const std::string_view logger_name = "root",
        SourceLocation source = {},
        const LogOrigin origin = LogOrigin::Application,
        const int rank = -1,
        const std::string_view language = "C++"
    ) {
        Logger::instance().warning(
            message,
            logger_name,
            std::move(source),
            origin,
            rank,
            language
        );
    }

    inline void info(
        const std::string_view message,
        const std::string_view logger_name = "root",
        SourceLocation source = {},
        const LogOrigin origin = LogOrigin::Application,
        const int rank = -1,
        const std::string_view language = "C++"
    ) {
        Logger::instance().info(
            message,
            logger_name,
            std::move(source),
            origin,
            rank,
            language
        );
    }

    inline void debug(
        const std::string_view message,
        const std::string_view logger_name = "root",
        SourceLocation source = {},
        const LogOrigin origin = LogOrigin::Application,
        const int rank = -1,
        const std::string_view language = "C++"
    ) {
        Logger::instance().debug(
            message,
            logger_name,
            std::move(source),
            origin,
            rank,
            language
        );
    }

    inline void critical(
        const std::string_view message,
        const std::string_view logger_name = "root",
        SourceLocation source = {},
        const LogOrigin origin = LogOrigin::Application,
        const int rank = -1,
        const std::string_view language = "C++"
    ) {
        Logger::instance().critical(
            message,
            logger_name,
            std::move(source),
            origin,
            rank,
            language
        );
    }
} // namespace owplog

/** Source location helper macro for call-site metadata injection. */
#define OWPLOG_SOURCE_LOCATION ::owplog::SourceLocation{__FILE__, __LINE__, __func__}

/** Convenience macro for full custom log call with explicit level/name/message. */
#define OWPLOG_LOG(level, logger_name, message) \
    ::owplog::Logger::instance().log((level), (message), (logger_name), OWPLOG_SOURCE_LOCATION)

/** Convenience macro for INFO-level messages on default logger name. */
#define OWPLOG_LOG_INFO_MSG(message) \
    ::owplog::Logger::instance().info((message), "owplog", OWPLOG_SOURCE_LOCATION)

/** Convenience macro for WARNING-level messages on default logger name. */
#define OWPLOG_LOG_WARNING_MSG(message) \
    ::owplog::Logger::instance().warning((message), "owplog", OWPLOG_SOURCE_LOCATION)

/** Convenience macro for ERROR-level messages on default logger name. */
#define OWPLOG_LOG_ERROR_MSG(message) \
    ::owplog::Logger::instance().error((message), "owplog", OWPLOG_SOURCE_LOCATION)

#endif
