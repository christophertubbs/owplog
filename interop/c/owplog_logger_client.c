#include "owplog_logger_client.h"

/**
 * @brief Register a named sublogger and return its handle.
 * @param sublogger_name Sublogger name text.
 * @return Positive handle on success, 0 on failure.
 * @pre `sublogger_name` should be non-null and non-empty.
 * @post Successful handle can be reused in handle-based log calls.
 * @error Returns `0` when registration fails.
 */
owplog_sublogger_handle owplog_logger_client_add_sublogger(const char* sublogger_name) {
    return owplog_logger_add_sublogger(sublogger_name);
}

/**
 * @brief Log INFO-level message with explicit logger name.
 * @param logger_name Logical logger name.
 * @param message Message text.
 * @pre Arguments may be null; C API will substitute defaults.
 * @post Message is forwarded to upstream logger or stdio fallback.
 */
void owplog_logger_client_log_info(const char* logger_name, const char* message) {
    owplog_logger_log(OWPLOG_LOG_INFO, logger_name, message);
}

/**
 * @brief Log WARNING-level message with explicit logger name.
 * @param logger_name Logical logger name.
 * @param message Message text.
 * @pre Arguments may be null; C API will substitute defaults.
 * @post Message is forwarded to upstream logger or stdio fallback.
 */
void owplog_logger_client_log_warning(const char* logger_name, const char* message) {
    owplog_logger_log(OWPLOG_LOG_WARNING, logger_name, message);
}

/**
 * @brief Log ERROR-level message with explicit logger name.
 * @param logger_name Logical logger name.
 * @param message Message text.
 * @pre Arguments may be null; C API will substitute defaults.
 * @post Message is forwarded to upstream logger or stdio fallback.
 */
void owplog_logger_client_log_error(const char* logger_name, const char* message) {
    owplog_logger_log(OWPLOG_LOG_ERROR, logger_name, message);
}

/**
 * @brief Log INFO-level message through a registered sublogger handle.
 * @param sublogger_handle Registered sublogger handle.
 * @param message Message text.
 * @pre `message` may be null and becomes empty payload.
 * @post Invalid handles are mapped to default logger name by C ABI implementation.
 */
void owplog_logger_client_log_sublogger_info(owplog_sublogger_handle sublogger_handle, const char* message) {
    owplog_logger_log_sublogger(OWPLOG_LOG_INFO, sublogger_handle, message);
}

/**
 * @brief Log INFO-level message with extended metadata.
 * @param logger_name Logical logger name.
 * @param message Message text.
 * @param meta Optional metadata pointer.
 * @pre `meta` may be null; pointer fields may be null.
 * @post Metadata is forwarded when upstream logger path is active.
 */
void owplog_logger_client_log_info_ex(
    const char* logger_name,
    const char* message,
    const owplog_log_meta* meta) {
    owplog_logger_log_ex(OWPLOG_LOG_INFO, logger_name, message, meta);
}
