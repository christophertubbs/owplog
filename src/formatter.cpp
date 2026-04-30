#include "owplog/formatter.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace owplog {

namespace {

/**
 * @brief Convert a thread id into string form.
 * @param id Thread id value.
 * @return String representation of thread id.
 */
std::string thread_id_string(const std::thread::id& id) {
    std::ostringstream os;
    os << id;
    return os.str();
}

/**
 * @brief Format a time point as local time with millisecond precision.
 * @param timestamp Input timestamp.
 * @return Formatted local timestamp string.
 */
std::string time_string(const std::chrono::system_clock::time_point& timestamp) {
    // Calendar-time value used for locale-aware date formatting.
    const auto as_time_t = std::chrono::system_clock::to_time_t(timestamp);
    // Local broken-down time structure filled using platform API.
    std::tm tm_value{};
#ifdef _WIN32
    localtime_s(&tm_value, &as_time_t);
#else
    localtime_r(&as_time_t, &tm_value);
#endif

    // Millisecond component used to extend %Y-%m-%d %H:%M:%S output.
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % std::chrono::seconds(1);

    std::ostringstream os;
    os << std::put_time(&tm_value, "%Y-%m-%d %H:%M:%S")
       << "." << std::setw(3) << std::setfill('0') << ms.count();
    return os.str();
}

/**
 * @brief Convert log origin enum to formatter token string.
 * @param origin Origin enum value.
 * @return Short origin label.
 */
const char* origin_string(LogOrigin origin) {
    switch (origin) {
        case LogOrigin::Application: return "app";
        case LogOrigin::CapturedStdout: return "stdout";
        case LogOrigin::CapturedStderr: return "stderr";
        case LogOrigin::CAbi: return "c_api";
        default: return "unknown";
    }
}

} // namespace

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
    std::ostringstream out;
    // Pattern index for walking the format template.
    for (std::size_t i = 0; i < this->pattern_.size(); ++i) {
        // Current template character.
        const char character = this->pattern_[i];
        if (character != '%' || i + 1 >= this->pattern_.size()) {
            out << character;
            continue;
        }

        // Token letter following `%`.
        const char token = this->pattern_[++i];
        switch (token) {
            case 'T': out << time_string(record.timestamp); break;
            case 'L': out << to_string(record.level); break;
            case 'N': out << record.logger_name; break;
            case 'm': out << record.message; break;
            case 't': out << thread_id_string(record.thread_id); break;
            case 'f': out << record.source.file; break;
            case 'l': out << record.source.line; break;
            case 'F': out << record.source.function; break;
            case 'o': out << origin_string(record.origin); break;
            case 'g':
                if (!record.language.empty()) {
                    out << record.language;
                } else {
                    out << "n/a";
                }
                break;
            case 'r':
                if (record.rank >= 0) {
                    out << record.rank;
                } else {
                    out << "n/a";
                }
                break;
            case '%': out << '%'; break;
            default:
                out << '%' << token;
                break;
        }
    }
    return out.str();
}

} // namespace owplog
