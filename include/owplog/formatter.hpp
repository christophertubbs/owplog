#ifndef OWPLOG_FORMATTER_HPP
#define OWPLOG_FORMATTER_HPP

/**
 * @file formatter.hpp
 * @brief Formatter interfaces and pattern-based formatter implementation.
 */
#include "owplog/log_record.hpp"

#include <memory>
#include <string>

namespace owplog {
    const std::string DEFAULT_PATTERN = "[%(asctime)s] %(levelname)s %(name)s %(filename)s::%(lineno)d - %(message)s";

    /**
     * @brief Base class for record-to-text formatting.
     * @note Implementations should be thread-safe because one formatter instance
     *       may be shared across worker dispatch cycles.
     */
    class LogFormatter {
    public:
        /** Virtual destructor for polymorphic formatter deletion. */
        virtual ~LogFormatter() = default;
        /**
         * @brief Convert a record to its serialized text representation.
         * @param record Record to format.
         * @return Formatted string output.
         * @pre `record` is a fully initialized log record.
         * @post Does not modify `record`.
         * @throws May throw implementation-defined formatting/allocation exceptions.
         */
        [[nodiscard]] virtual std::string format(const LogRecord& record) const = 0;
    };

    /**
     * @brief Formatter supporting tokenized string patterns.
     */
    class PatternFormatter : public LogFormatter {
    public:
        /**
         * @brief Construct a pattern formatter.
         * @param pattern Pattern string containing `%` tokens.
         * @pre `pattern` may be empty; unknown tokens are emitted literally.
         * @post Formatter stores the provided pattern.
         * @throws May propagate allocation failures while storing pattern text.
         */
        explicit PatternFormatter(
            std::string pattern = DEFAULT_PATTERN
        );
        /**
         * @brief Format one record according to configured token pattern.
         * @param record Record to format.
         * @return Final formatted string.
         * @pre `record` contains values for tokens referenced by the pattern.
         * @post Returns a single-line payload without trailing newline.
         * @throws May propagate allocation failures and stream formatting exceptions.
         */
        [[nodiscard]] std::string format(const LogRecord& record) const override;

    private:
        /** Format string template with token placeholders. */
        std::string pattern_;
        std::string datefmt_;
    };

    /** Shared-pointer alias for formatter objects. */
    using LogFormatterPtr = std::shared_ptr<LogFormatter>;

} // namespace owplog

#endif
