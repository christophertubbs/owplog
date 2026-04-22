#ifndef OWPLOG_CXX_LOGGER_HPP
#define OWPLOG_CXX_LOGGER_HPP

/**
 * @file owplog_cxx_logger.hpp
 * @brief Small C++ wrapper around owplog message submission C ABI.
 */

#include "owplog/c_api.h"

#include <string>

namespace owplog {
namespace interop {

/**
 * @brief Thin C++ wrapper for C ABI message submission and subloggers.
 * @note This wrapper is stateless and delegates directly to C ABI functions.
 */
class CApiLogger final {
public:
    /**
     * @brief Register named sublogger and return handle.
     * @param sublogger_name Name to register.
     * @return Positive handle on success, `0` on failure.
     * @pre `sublogger_name` should not be empty.
     * @post Returned handle can be reused with @ref log_sublogger.
     */
    owplog_sublogger_handle add_sublogger(const std::string& sublogger_name) const {
        return owplog_logger_add_sublogger(sublogger_name.c_str());
    }

    /**
     * @brief Submit one message using explicit logger name.
     * @param level Numeric level compatible with C ABI constants.
     * @param logger_name Logical logger name.
     * @param message Message payload.
     * @pre None.
     * @post Record is forwarded to upstream logger or fallback stdio path.
     */
    void log(int level, const std::string& logger_name, const std::string& message) const {
        owplog_logger_log(level, logger_name.c_str(), message.c_str());
    }

    /**
     * @brief Submit one message using explicit logger name and metadata.
     * @param level Numeric level compatible with C ABI constants.
     * @param logger_name Logical logger name.
     * @param message Message payload.
     * @param meta Optional metadata fields (file/line/function/language/rank).
     * @pre Any pointer fields in `meta` must remain valid for the duration of the call.
     * @post Metadata is forwarded when upstream logger path is active.
     */
    void log_ex(
        int level,
        const std::string& logger_name,
        const std::string& message,
        const owplog_log_meta& meta) const {
        owplog_logger_log_ex(level, logger_name.c_str(), message.c_str(), &meta);
    }

    /**
     * @brief Submit one message using a registered sublogger handle.
     * @param level Numeric level compatible with C ABI constants.
     * @param sublogger_handle Handle returned by @ref add_sublogger.
     * @param message Message payload.
     * @pre None.
     * @post Invalid handles are mapped to default logger name by C ABI implementation.
     */
    void log_sublogger(int level, owplog_sublogger_handle sublogger_handle, const std::string& message) const {
        owplog_logger_log_sublogger(level, sublogger_handle, message.c_str());
    }
};

} // namespace interop
} // namespace owplog

#endif
