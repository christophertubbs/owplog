#include "owplog/formatter.hpp"

namespace owplog {
    /**
     * @brief Construct pattern formatter.
     * @param pattern Pattern containing `%` replacement tokens.
     */
    PatternFormatter::PatternFormatter(std::string pattern)
        : pattern_(std::move(pattern)) {}

    /**
     * @brief Format a record using token replacement.
     * @param record Record to format.
     * @return Final formatted string.
     */
    std::string PatternFormatter::format(const LogRecord& record) const {
        return format_log_record(record, this->pattern_);
    }
} // namespace owplog
