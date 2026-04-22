#include "owplog/filter.hpp"

#include <utility>

namespace owplog {

/**
 * @brief Construct a level-threshold filter.
 * @param min_level Minimum allowed severity level.
 */
LevelFilter::LevelFilter(LogLevel min_level)
    : min_level_(min_level) {}

/**
 * @brief Allow records at or above configured threshold.
 * @param record Record under evaluation.
 * @return True when record level satisfies threshold.
 */
bool LevelFilter::allow(const LogRecord& record) const {
    return static_cast<int>(record.level) >= static_cast<int>(min_level_);
}

/**
 * @brief Construct a substring matching filter.
 * @param token Text token to search in message/logger name.
 */
SubstringFilter::SubstringFilter(std::string token)
    : token_(std::move(token)) {}

/**
 * @brief Allow records containing configured token.
 * @param record Record under evaluation.
 * @return True when token is present or empty.
 */
bool SubstringFilter::allow(const LogRecord& record) const {
    if (token_.empty()) {
        return true;
    }
    return record.message.find(token_) != std::string::npos
        || record.logger_name.find(token_) != std::string::npos;
}

} // namespace owplog
