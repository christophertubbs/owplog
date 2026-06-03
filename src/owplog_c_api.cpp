#include "owplog/c_api.h"

#include "owplog/owplog.h"

#include <iostream>
#include <mutex>
#include <string>
#include <vector>

namespace {

/** Mutex protecting fallback stdout/stderr writes. */
std::mutex g_fallback_mutex;
/** Mutex protecting the sublogger name registry. */
std::mutex g_sublogger_mutex;
/** 1-based handle registry for sublogger names. */
std::vector<std::string> g_sublogger_names;

/**
 * @brief Convert C ABI integer level to owplog enum level.
 * @param level Numeric C ABI level.
 * @return Mapped owplog level enum.
 */
owplog::LogLevel as_level(int level) {
    return owplog::from_int(level);
}

/**
 * @brief Check whether an upstream owplog logger has sinks configured.
 * @return True when records can be forwarded to configured sinks.
 */
bool has_upstream_logger() {
    return owplog::has_handlers();
}

/**
 * @brief Write fallback output to stdout/stderr when no upstream logger exists.
 * @param level Record severity.
 * @param logger_name Logger name used for prefix.
 * @param message Message text.
 */
void fallback_emit(owplog::LogLevel level, const std::string& logger_name, const std::string& message) {
    std::lock_guard<std::mutex> lock(g_fallback_mutex);
    std::ostream& out = (static_cast<int>(level) >= static_cast<int>(owplog::LogLevel::Error)) ? std::cerr : std::cout;
    out << "[" << owplog::to_string(level) << "] "
        << "(" << logger_name << ") "
        << message << '\n';
    out.flush();
}

/**
 * @brief Submit to upstream logger or fallback output, depending on availability.
 * @param level Record severity.
 * @param logger_name Logger name.
 * @param message Message text.
 * @param source Optional source metadata.
 * @param rank Optional rank metadata.
 * @param language Optional caller language label.
 */
void submit_or_fallback(
    owplog::LogLevel level,
    const std::string& logger_name,
    const std::string& message,
    const owplog::SourceLocation& source,
    int rank,
    const std::string& language) {
    if (level == owplog::LogLevel::Off) {
        return;
    }

    if (has_upstream_logger()) {
        owplog::log(level, message, logger_name, source, owplog::LogOrigin::CAbi, rank, language);
        return;
    }

    // Fallback path cannot persist full metadata, but still emits the message.
    fallback_emit(level, logger_name, message);
}

/**
 * @brief Convert optional C metadata to SourceLocation.
 * @param meta Optional metadata pointer.
 * @return SourceLocation extracted from metadata.
 */
owplog::SourceLocation source_from_meta(const owplog_log_meta* meta) {
    owplog::SourceLocation source{};
    if (meta == nullptr) {
        return source;
    }
    if (meta->file != nullptr) {
        source.file = meta->file;
    }
    source.line = meta->line;
    if (meta->function != nullptr) {
        source.function = meta->function;
    }
    return source;
}

/**
 * @brief Extract optional rank from metadata.
 * @param meta Optional metadata pointer.
 * @return Rank value or -1.
 */
int rank_from_meta(const owplog_log_meta* meta) {
    if (meta == nullptr) {
        return -1;
    }
    return meta->rank;
}

/**
 * @brief Extract optional language label from metadata.
 * @param meta Optional metadata pointer.
 * @return Language string or empty string.
 */
std::string language_from_meta(const owplog_log_meta* meta) {
    if (meta == nullptr || meta->language == nullptr) {
        return "";
    }
    return meta->language;
}

/**
 * @brief Resolve a 1-based sublogger handle to name.
 * @param sublogger_handle Handle returned by owplog_logger_add_sublogger.
 * @return Resolved name or default name when handle is invalid.
 */
std::string resolve_sublogger_name(owplog_sublogger_handle sublogger_handle) {
    std::lock_guard<std::mutex> lock(g_sublogger_mutex);
    if (sublogger_handle <= 0) {
        return "owplog.c";
    }
    const auto index = static_cast<std::size_t>(sublogger_handle - 1);
    if (index >= g_sublogger_names.size()) {
        return "owplog.c";
    }
    return g_sublogger_names[index];
}

} // namespace

extern "C" {

/**
 * @brief Register or resolve a sublogger handle for a given name.
 * @param sublogger_name Null-terminated sublogger name.
 * @return Positive 1-based handle on success, 0 on invalid input.
 * @pre `sublogger_name` should be non-null and non-empty.
 * @post Same `sublogger_name` maps deterministically to the same handle.
 */
owplog_sublogger_handle owplog_logger_add_sublogger(const char* sublogger_name) {
    if (sublogger_name == nullptr || *sublogger_name == '\0') {
        return 0;
    }

    std::lock_guard<std::mutex> lock(g_sublogger_mutex);
    const std::string name(sublogger_name);
    for (std::size_t i = 0; i < g_sublogger_names.size(); ++i) {
        if (g_sublogger_names[i] == name) {
            return static_cast<owplog_sublogger_handle>(i + 1);
        }
    }

    g_sublogger_names.push_back(name);
    return static_cast<owplog_sublogger_handle>(g_sublogger_names.size());
}

/**
 * @brief Submit one C ABI record using explicit logger name.
 * @param level Numeric level from C API constants.
 * @param logger_name Nullable logger name; defaults to `owplog.c`.
 * @param message Nullable message text; defaults to empty string.
 * @post Record is routed to upstream logger when available, else stdio fallback.
 */
void owplog_logger_log(int level, const char* logger_name, const char* message) {
    owplog_logger_log_ex(level, logger_name, message, nullptr);
}

/**
 * @brief Submit one C ABI record with optional metadata.
 * @param level Numeric level from C API constants.
 * @param logger_name Nullable logger name.
 * @param message Nullable message text.
 * @param meta Nullable metadata pointer.
 * @post Optional metadata is forwarded on upstream logger path.
 */
void owplog_logger_log_ex(int level, const char* logger_name, const char* message, const owplog_log_meta* meta) {
    const std::string safe_logger_name = (logger_name != nullptr && *logger_name != '\0') ? logger_name : "owplog.c";
    const std::string safe_message = (message != nullptr) ? message : "";
    submit_or_fallback(
        as_level(level),
        safe_logger_name,
        safe_message,
        source_from_meta(meta),
        rank_from_meta(meta),
        language_from_meta(meta));
}

/**
 * @brief Submit one C ABI record using a sublogger handle.
 * @param level Numeric level from C API constants.
 * @param sublogger_handle Handle returned by `owplog_logger_add_sublogger`.
 * @param message Nullable message text.
 * @post Invalid handles degrade to default logger name.
 */
void owplog_logger_log_sublogger(int level, owplog_sublogger_handle sublogger_handle, const char* message) {
    const std::string safe_logger_name = resolve_sublogger_name(sublogger_handle);
    const std::string safe_message = (message != nullptr) ? message : "";
    submit_or_fallback(
        as_level(level),
        safe_logger_name,
        safe_message,
        {},
        -1,
        "");
}

void owplog_logger_log_full(
    const int level,
    const char* logger_name,
    const char* message,
    const char* language,
    const char* filename,
    int line_number,
    const char* function_name,
    int rank
) {
    const std::string safe_logger_name = (logger_name != nullptr && *logger_name != '\0') ? logger_name : "owplog.c";
    const std::string safe_message = (message != nullptr) ? message : "";
    const owplog::SourceLocation source{filename, line_number, function_name};
    submit_or_fallback(
        as_level(level),
        safe_logger_name,
        safe_message,
        source,
        rank,
        language
    );
}

} // extern "C"
