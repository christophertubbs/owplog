#ifndef OWPLOG_C_API_H
#define OWPLOG_C_API_H

/**
 * @file c_api.h
 * @brief C ABI for submitting messages to owplog.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Log levels accepted by the owplog C API.
 */
typedef enum owplog_log_level {
    /** Trace-level diagnostic output. */
    OWPLOG_LOG_TRACE = 0,
    /** Debug-level diagnostic output. */
    OWPLOG_LOG_DEBUG = 10,
    /** General informational output. */
    OWPLOG_LOG_INFO = 20,
    /** Warning-level output. */
    OWPLOG_LOG_WARNING = 30,
    /** Error-level output. */
    OWPLOG_LOG_ERROR = 40,
    /** Critical/fatal-level output. */
    OWPLOG_LOG_CRITICAL = 50,
    /** Disable logging output entirely. */
    OWPLOG_LOG_OFF = 60
} owplog_log_level;

/** Opaque integer handle returned when registering a named sublogger. */
typedef int owplog_sublogger_handle;

/**
 * @brief Optional metadata for extended C-API log submission.
 *
 * All pointer fields are nullable. Set unknown numeric fields to:
 * - line: 0
 * - rank: -1
 *
 * @note Callers own pointed-to memory and must keep it valid for the duration
 *       of the call to `owplog_logger_log_ex`.
 */
typedef struct owplog_log_meta {
    /** Optional source filename. */
    const char* file;
    /** Optional 1-based source line number (0 when unknown). */
    int line;
    /** Optional function/procedure name. */
    const char* function;
    /** Optional language/runtime label (for example "python", "fortran"). */
    const char* language;
    /** Optional rank metadata (-1 when unknown). */
    int rank;
} owplog_log_meta;

/**
 * @brief Register a named sublogger for handle-based logging.
 *
 * This call does not initialize or reconfigure the owplog singleton.
 *
 * @param sublogger_name Sublogger name (for example, "solver.rank_0003").
 * @return Positive handle on success, 0 on failure.
 * @pre `sublogger_name` points to a null-terminated UTF-8 compatible string.
 * @post On success, returned handle can be reused in `owplog_logger_log_sublogger`.
 * @post Re-registering the same name returns the existing handle.
 * @error Returns `0` when `sublogger_name` is null/empty or registration fails.
 */
owplog_sublogger_handle owplog_logger_add_sublogger(const char* sublogger_name);

/**
 * @brief Submit one log message using an explicit logger name.
 *
 * If no upstream owplog logger is configured with sinks, this call writes
 * directly to stdout/stderr as a fallback.
 *
 * @param level One of @ref owplog_log_level values.
 * @param logger_name Logical logger name (e.g. module/component).
 * @param message Message body text.
 * @pre `logger_name` and `message` may be null; defaults are applied internally.
 * @post Message is queued to upstream logger when sinks exist; otherwise emitted to stdio fallback.
 * @error Invalid numeric `level` values are clamped to nearest owplog threshold.
 */
void owplog_logger_log(int level, const char* logger_name, const char* message);

/**
 * @brief Submit one log message using a previously registered sublogger handle.
 *
 * If no upstream owplog logger is configured with sinks, this call writes
 * directly to stdout/stderr as a fallback.
 *
 * @param level One of @ref owplog_log_level values.
 * @param sublogger_handle Handle returned by @ref owplog_logger_add_sublogger.
 * @param message Message body text.
 * @pre `message` may be null; empty message is emitted in that case.
 * @post Valid handles use registered names; invalid handles use default `"owplog.c"`.
 * @error Invalid numeric `level` values are clamped to nearest owplog threshold.
 */
void owplog_logger_log_sublogger(int level, owplog_sublogger_handle sublogger_handle, const char* message);

/**
 * @brief Submit one log message with optional extended metadata.
 *
 * If no upstream owplog logger is configured with sinks, this call writes
 * directly to stdout/stderr as a fallback.
 *
 * @param level One of @ref owplog_log_level values.
 * @param logger_name Logical logger name (e.g. module/component).
 * @param message Message body text.
 * @param meta Optional pointer to metadata; pass null if unavailable.
 * @pre `meta` fields are optional; null pointers and sentinel values are accepted.
 * @post Metadata is forwarded when upstream logger exists; fallback stdio output ignores metadata fields.
 * @error Invalid numeric `level` values are clamped to nearest owplog threshold.
 */
void owplog_logger_log_ex(
    int level,
    const char* logger_name,
    const char* message,
    const owplog_log_meta* meta
);

void owplog_logger_log_full(
    int level,
    const char* logger_name,
    const char* message,
    const char* language,
    const char* filename,
    int line_number,
    const char* function_name,
    int rank
);

#ifdef __cplusplus
}
#endif

#define LOG(level, logger_name, message)                                    \
    do {                                                                    \
    owplog_log_meta metadata = {__FILE__, __LINE__, __func__, "", -1};      \
    owplog_logger_log_ex((level), (logger_name), (message), &metadata);     \
} while (0)
#define LOG_NAMED_INFO(logger_name, message) LOG(OWPLOG_LOG_INFO, logger_name, message)
#define LOG_NAMED_WARNING(logger_name, message) LOG(OWPLOG_LOG_WARNING, logger_name, message)
#define LOG_NAMED_ERROR(logger_name, message) LOG(OWPLOG_LOG_ERROR, logger_name, message)
#define LOG_NAMED_DEBUG(logger_name, message) LOG(OWPLOG_LOG_DEBUG, logger_name, message)
#define LOG_INFO(message) LOG_NAMED_INFO("owplog", message)
#define LOG_WARNING(message) LOG_NAMED_WARNING("owplog", message)
#define LOG_ERROR(message) LOG_NAMED_ERROR("owplog", message)
#define LOG_DEBUG(message) LOG_NAMED_DEBUG("owplog", message)
#endif
