#ifndef OWPLOG_LOGGER_CLIENT_H
#define OWPLOG_LOGGER_CLIENT_H

/**
 * @file owplog_logger_client.h
 * @brief Thin C convenience wrapper for owplog message submission API.
 */

#include "owplog/c_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register a named sublogger and return its handle.
 * @param sublogger_name Sublogger name text.
 * @return Positive handle on success, 0 on failure.
 * @pre `sublogger_name` must be a null-terminated string.
 * @post Successful handle can be reused for sublogger logging calls.
 * @error Returns `0` for null/empty names or internal failure.
 */
owplog_sublogger_handle owplog_logger_client_add_sublogger(const char* sublogger_name);

/**
 * @brief Log INFO message using explicit logger name.
 * @param logger_name Logger name text.
 * @param message Message text.
 * @pre Arguments may be null; underlying API applies safe defaults.
 * @post Message is forwarded to upstream logger or emitted to stdio fallback.
 * @error No direct return code; use process logs for operational diagnostics.
 */
void owplog_logger_client_log_info(const char* logger_name, const char* message);

/**
 * @brief Log WARNING message using explicit logger name.
 * @param logger_name Logger name text.
 * @param message Message text.
 * @pre Arguments may be null; underlying API applies safe defaults.
 * @post Message is forwarded to upstream logger or emitted to stdio fallback.
 * @error No direct return code.
 */
void owplog_logger_client_log_warning(const char* logger_name, const char* message);

/**
 * @brief Log ERROR message using explicit logger name.
 * @param logger_name Logger name text.
 * @param message Message text.
 * @pre Arguments may be null; underlying API applies safe defaults.
 * @post Message is forwarded to upstream logger or emitted to stdio fallback.
 * @error No direct return code.
 */
void owplog_logger_client_log_error(const char* logger_name, const char* message);

/**
 * @brief Log INFO message using a registered sublogger handle.
 * @param sublogger_handle Handle from @ref owplog_logger_client_add_sublogger.
 * @param message Message text.
 * @pre `message` may be null and becomes an empty payload.
 * @post Invalid handles are mapped to default logger name by the C ABI.
 * @error No direct return code.
 */
void owplog_logger_client_log_sublogger_info(owplog_sublogger_handle sublogger_handle, const char* message);

/**
 * @brief Log INFO message with extended metadata.
 * @param logger_name Logger name text.
 * @param message Message text.
 * @param meta Optional metadata pointer.
 * @pre `meta` may be null; each metadata field is optional.
 * @post Metadata is forwarded when upstream logger path is active.
 * @error No direct return code.
 */
void owplog_logger_client_log_info_ex(
    const char* logger_name,
    const char* message,
    const owplog_log_meta* meta);

/**
 * @brief Convenience macro for C call sites that want source metadata.
 * @pre Used inside a C function scope (requires `__func__` support).
 * @post Emits one INFO record with call-site source metadata and user-provided language/rank.
 */
#define OWPLOG_LOG_INFO_EX_C(logger_name, message, language_label, rank_value) \
    do { \
        const owplog_log_meta owplog_meta_ = {__FILE__, __LINE__, __func__, (language_label), (rank_value)}; \
        owplog_logger_client_log_info_ex((logger_name), (message), &owplog_meta_); \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif
