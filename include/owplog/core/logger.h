//
// Created by christopher.tubbs on 6/1/26.
//

#ifndef OWPLOG_LOGGER_H
#define OWPLOG_LOGGER_H

#include <unistd.h>
#include <string>
#include <optional>
#include <vector>

#include "owplog/log_level.hpp"
#include "owplog/handlers/handlers.h"
#include "owplog/log_record.hpp"
#include "owplog/formatter.hpp"

namespace owplog::core {
    struct LoggerConfigurationArguments {
        std::optional<std::string> filename;
        std::optional<std::string> format;
        std::optional<std::string> datefmt;
        std::optional<std::vector<handlers::LogHandlerPtr>> handlers;
        bool force = false;
        std::optional<LogLevel> level;
    };

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
             * @brief Stop worker thread and optional stdio capture.
             * @pre None.
             * @post Worker thread is stopped and stdio capture is inactive.
             * @note Safe to call multiple times.
             */
            static void shutdown();

            /**
             * @brief Set process-level minimum accepted level.
             * @param level Minimum severity threshold.
             * @pre `level` is a valid @ref LogLevel value.
             * @post Future records below `level` are dropped before queueing.
             * @throws No intentional exceptions.
             */
            static void set_level(LogLevel level);
            /**
             * @brief Read current process-level threshold.
             * @return Configured level threshold.
             * @pre None.
             * @post Logger state is unchanged.
             * @throws No intentional exceptions.
             */
            static LogLevel level();

            /**
             * @brief Replace active formatter.
             * @param formatter Formatter instance (null restores default pattern formatter).
             * @pre None.
             * @post Formatter used by subsequent records is updated.
             * @throws May propagate allocation failures when restoring default formatter.
             */
            static void set_formatter(LogFormatterPtr formatter);

            /**
             * @brief Replace active formatter with a basic string formatter
             * @param pattern The pattern string for a string pattern formatter
             */
            static void set_formatter(const std::string& pattern);
            static void set_formatter(const char* pattern);
            /**
             * @brief Add a sink destination.
             * @param handler Handler instance to append.
             * @pre `sink` should be non-null for useful output.
             * @post Handler list contains appended sink pointer.
             * @throws May propagate allocation failures when growing sink storage.
             */
            static void add_handler(handlers::LogHandlerPtr handler);
            /**
             * @brief Remove all configured sinks.
             * @pre None.
             * @post Handler list is empty.
             * @throws No intentional exceptions.
             */
            static void clear_handlers();

            /**
             * @brief Add a process-level filter.
             * @param filter Filter to append.
             * @pre `filter` should be non-null for useful filtering.
             * @post Filter list contains appended filter pointer.
             * @throws May propagate allocation failures when growing filter storage.
             */
            static void add_filter(LogFilterPtr filter);
            /**
             * @brief Remove all process-level filters.
             * @pre None.
             * @post Process-level filter list is empty.
             * @throws No intentional exceptions.
             */
            static void clear_filters();
            /**
             * @brief Check whether stdio capture is active.
             * @return True when capture helper is active.
             * @pre None.
             * @post Logger state is unchanged.
             * @throws No intentional exceptions.
             */
            bool stdio_capture_active();
            /**
             * @brief Check whether the logger currently has configured sinks.
             * @return True when at least one handler is configured.
             * @pre None.
             * @post Logger state is unchanged.
             * @throws No intentional exceptions.
             */
            static bool has_handlers();
            static bool is_configured();

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
            static void log(
                LogLevel level,
                std::string_view message,
                std::string_view logger_name = "owplog",
                SourceLocation source = {},
                LogOrigin origin = LogOrigin::Application,
                int rank = -1,
                std::string_view language = "C++"
            );

            void log_message(
                LogLevel level,
                std::string_view message,
                std::string_view logger_name = "owplog",
                SourceLocation source = {},
                LogOrigin origin = LogOrigin::Application,
                int rank = -1,
                std::string_view language = "C++"
                );

            static void error(
                const std::string_view message,
                const std::string_view logger_name = "owplog",
                SourceLocation source = {},
                const LogOrigin origin = LogOrigin::Application,
                const int rank = -1,
                const std::string_view language = "C++"
            ) {
                log(LogLevel::Error, message, logger_name, std::move(source), origin, rank, language);
            }

            static void warning(
                const std::string_view message,
                const std::string_view logger_name = "owplog",
                SourceLocation source = {},
                const LogOrigin origin = LogOrigin::Application,
                const int rank = -1,
                const std::string_view language = "C++"
            ) {
                log(LogLevel::Warning, message, logger_name, std::move(source), origin, rank, language);
            }

            static void info(
                const std::string_view message,
                const std::string_view logger_name = "owplog",
                SourceLocation source = {},
                const LogOrigin origin = LogOrigin::Application,
                const int rank = -1,
                const std::string_view language = "C++"
            ) {
                log(LogLevel::Info, message, logger_name, std::move(source), origin, rank, language);
            }

            static void debug(
                const std::string_view message,
                const std::string_view logger_name = "owplog",
                SourceLocation source = {},
                const LogOrigin origin = LogOrigin::Application,
                const int rank = -1,
                const std::string_view language = "C++"
            ) {
                log(LogLevel::Debug, message, logger_name, std::move(source), origin, rank, language);
            }

            static void critical(
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
            static void flush();

            /** Deleted copy constructor for singleton semantics. */
            Logger(const Logger&) = delete;
            /** Deleted copy assignment for singleton semantics. */
            Logger& operator=(const Logger&) = delete;

            static void configure(const LoggerConfigurationArguments& configuration) {
                instance().configure_internal(configuration);
            };

        private:

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
            void stop_stdio_capture() const;
            void internal_shutdown();
            void internal_start();
            void flush_internal();
            void configure_internal(const LoggerConfigurationArguments& configuration);
            Logger() : Logger(LoggerConfigurationArguments{}) {}
            explicit Logger(const LoggerConfigurationArguments& configuration);
            /** @brief Ensure a graceful shutdown on process teardown. */
            ~Logger();
            static Logger& instance();

            /** @brief Worker thread main loop. */
            void worker_loop();
            /**
             * @brief Evaluate process-level filters.
             * @param record Record under evaluation.
             * @return True when allowed by all filters.
             */
            bool passes_filters(const LogRecord& record) const;
            /**
             * @brief Push record into a queue and notify a worker.
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
            /** True when a worker thread is running. */
            std::atomic<bool> running_ = false;
            /** Shutdown request flag observed by worker loop. */
            std::atomic<bool> stop_requested_ = false;
            std::atomic<bool> is_configured_ = false;
            std::string datefmt = "%Y-%m-%d %H:%M:%S%z";

            /** Optional stdio capture subsystem instance. */
            std::unique_ptr<StdioCapture> stdio_capture_;
    };

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

    class LoggerStdioCapture {
    public:
        /**
         * @brief Construct capture helper.
         * @param logger Logger used for forwarding captured lines.
         */
        explicit LoggerStdioCapture(Logger& logger) : logger_(logger), running_(false) {}

        /** Ensure capture is stopped during destruction. */
        ~LoggerStdioCapture();

        /**
         * @brief Start capture threads for selected streams.
         * @param capture_stdout Whether stdout should be captured.
         * @param capture_stderr Whether stderr should be captured.
         * @return True on success.
         */
        bool start(const bool capture_stdout, const bool capture_stderr);

        /**
         * @brief Stop capture threads and restore original descriptors.
         */
        void stop();

        /**
         * @brief Query capture active state.
         * @return True when capture helper is running.
         */
        [[nodiscard]] bool active() const {
            return running_;
        }

    private:

        /**
         * @brief Start redirection for one stream.
         * @param state Mutable stream state slot.
         * @param stream C stdio stream pointer.
         * @param origin Origin metadata for forwarded records.
         * @param label Logger name for forwarded records.
         * @return True on success.
         */
        bool listen_to_stream(StreamState& state, FILE* stream, const LogOrigin origin, const std::string& label) const;

        /**
         * @brief Stop one stream redirection and restore original descriptor.
         * @param state Mutable stream state slot.
         */
        static void stop_listening(StreamState& state);

        /** Logger used to emit captured lines. */
        Logger& logger_;
        /** Atomic running flag for quick active checks. */
        std::atomic<bool> running_;
        /** State for stdout capture path. */
        StreamState stdout_state_;
        /** State for stderr capture path. */
        StreamState stderr_state_;
    };

    class Logger::StdioCapture : public LoggerStdioCapture {
    public:
        /** @brief Construct logger stdio capture bridge. */
        explicit StdioCapture(Logger& logger)
            : LoggerStdioCapture(logger) {}
    };
}
#endif //OWPLOG_LOGGER_H
