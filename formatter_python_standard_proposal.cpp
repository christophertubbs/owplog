#include "owplog/formatter.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>

/*
 * Proposal: make PatternFormatter accept Python logging-style field names.
 *
 * Examples:
 *   "%(asctime)s [%(levelname)s] (%(name)s:%(process)s:%(thread)s) %(message)s"
 *   "%(asctime)s [%(levelname)s/%(levelno)s] (%(name)s) %(filename)s:%(lineno)s %(funcName)s rank=%(rank)s lang=%(language)s %(message)s"
 */
namespace owplog {

namespace {

std::string thread_id_string(const std::thread::id& thread_id) {
    std::ostringstream output;
    output << thread_id;
    return output.str();
}

std::string time_string(const std::chrono::system_clock::time_point& timestamp) {
    const auto calendar_time = std::chrono::system_clock::to_time_t(timestamp);
    std::tm local_time{};
#ifdef _WIN32
    localtime_s(&local_time, &calendar_time);
#else
    localtime_r(&calendar_time, &local_time);
#endif

    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % std::chrono::seconds(1);

    std::ostringstream output;
    output << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S")
           << "." << std::setw(3) << std::setfill('0') << milliseconds.count();
    return output.str();
}

const char* origin_string(LogOrigin origin) {
    switch (origin) {
        case LogOrigin::Application: return "app";
        case LogOrigin::CapturedStdout: return "stdout";
        case LogOrigin::CapturedStderr: return "stderr";
        case LogOrigin::CAbi: return "c_api";
        default: return "unknown";
    }
}

int level_number(LogLevel level) {
    return static_cast<int>(level);
}

std::string field_value(const LogRecord& record, const std::string& field_name) {
    if (field_name == "asctime") {
        return time_string(record.timestamp);
    }
    if (field_name == "levelname") {
        return to_string(record.level);
    }
    if (field_name == "levelno") {
        return std::to_string(level_number(record.level));
    }
    if (field_name == "name") {
        return record.logger_name;
    }
    if (field_name == "message") {
        return record.message;
    }
    if (field_name == "process") {
        return std::to_string(static_cast<long>(getpid()));
    }
    if (field_name == "thread") {
        return thread_id_string(record.thread_id);
    }
    if (field_name == "filename" || field_name == "pathname") {
        return record.source.file;
    }
    if (field_name == "lineno") {
        return std::to_string(record.source.line);
    }
    if (field_name == "funcName") {
        return record.source.function;
    }
    if (field_name == "origin") {
        return origin_string(record.origin);
    }
    if (field_name == "language") {
        return record.language.empty() ? "n/a" : record.language;
    }
    if (field_name == "rank") {
        return record.rank >= 0 ? std::to_string(record.rank) : "n/a";
    }

    return "%(" + field_name + ")s";
}

} // namespace

std::string PatternFormatter::format(const LogRecord& record) const {
    std::ostringstream output;

    for (std::size_t index = 0; index < this->pattern_.size(); ++index) {
        const char character = this->pattern_[index];
        if (character != '%') {
            output << character;
            continue;
        }

        if (index + 1 >= this->pattern_.size()) {
            output << character;
            continue;
        }

        if (this->pattern_[index + 1] != '(') {
            output << character;
            continue;
        }

        const std::size_t field_name_begin = index + 2;
        const std::size_t closing_parenthesis = this->pattern_.find(')', field_name_begin);
        if (closing_parenthesis == std::string::npos || closing_parenthesis + 1 >= this->pattern_.size()) {
            output << character;
            continue;
        }

        const char conversion_type = this->pattern_[closing_parenthesis + 1];
        if (conversion_type != 's') {
            output << this->pattern_.substr(index, closing_parenthesis + 2 - index);
            index = closing_parenthesis + 1;
            continue;
        }

        const std::string field_name = this->pattern_.substr(
            field_name_begin,
            closing_parenthesis - field_name_begin
        );
        output << field_value(record, field_name);
        index = closing_parenthesis + 1;
    }

    return output.str();
}

} // namespace owplog
