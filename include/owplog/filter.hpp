#ifndef OWPLOG_FILTER_HPP
#define OWPLOG_FILTER_HPP

/**
 * @file filter.hpp
 * @brief Filter interfaces and concrete filter implementations.
 */

#include "owplog/log_record.hpp"

#include <memory>
#include <string>

namespace owplog {

/**
 * @brief Base class for runtime record filtering.
 * @note Filter instances may be reused across threads and sinks.
 */
class LogFilter {
public:
    /** Virtual destructor for polymorphic filter deletion. */
    virtual ~LogFilter() = default;
    /**
     * @brief Decide whether a record should be processed.
     * @param record Record under evaluation.
     * @return True if record is allowed.
     * @pre `record` is valid for read-only inspection.
     * @post Does not modify `record`.
     * @throws Implementations should avoid throwing; if they do, dispatch may terminate.
     */
    virtual bool allow(const LogRecord& record) const = 0;
};

/**
 * @brief Filter that keeps records at or above a minimum level.
 */
class LevelFilter final : public LogFilter {
public:
    /**
     * @brief Construct a minimum-level filter.
     * @param min_level Minimum allowed level.
     * @pre `min_level` is a valid @ref LogLevel enumerator.
     * @post Filter stores the threshold for subsequent `allow()` calls.
     * @throws No intentional exceptions.
     */
    explicit LevelFilter(LogLevel min_level);
    /**
     * @brief Apply level-threshold filtering.
     * @param record Record under evaluation.
     * @return True when record level is >= configured minimum.
     * @pre None.
     * @post Filter state is unchanged.
     * @throws No intentional exceptions.
     */
    bool allow(const LogRecord& record) const override;

private:
    /** Minimum allowed level threshold. */
    LogLevel min_level_;
};

/**
 * @brief Filter that keeps records containing a text token.
 */
class SubstringFilter final : public LogFilter {
public:
    /**
     * @brief Construct a token-matching filter.
     * @param token Token searched in logger name and message.
     * @pre None.
     * @post Filter stores token text for subsequent `allow()` calls.
     * @throws May propagate allocation failures while storing token text.
     */
    explicit SubstringFilter(std::string token);
    /**
     * @brief Apply token matching to logger name/message.
     * @param record Record under evaluation.
     * @return True when token is present or token is empty.
     * @pre None.
     * @post Filter state is unchanged.
     * @throws No intentional exceptions.
     */
    bool allow(const LogRecord& record) const override;

private:
    /** Token used for substring matching. */
    std::string token_;
};

/** Shared-pointer alias for filter objects. */
using LogFilterPtr = std::shared_ptr<LogFilter>;

} // namespace owplog

#endif
