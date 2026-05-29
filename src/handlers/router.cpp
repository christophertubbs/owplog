//
// Created by christopher.tubbs on 5/29/26.
//
#include <iostream>
#include "owplog/handlers/router.h"
#include "owplog/handlers/console_handler.h"
#include "owplog/handlers/file_handler.h"

#include "owplog/utility.hpp"

namespace owplog::handlers {
    template<typename ... Args>
    LogRouter<Args...>::LogRouter(
        const std::string_view& name_pattern,
        const std::string_view& handler_type,
        std::vector<std::string_view>& discriminators,
        Args... args
    ) {

        if (handler_type != "FileHandler") {
            std::ostringstream error_builder;
            error_builder << "'" << handler_type << "' is not a valid handler type. Valid types are: FileHandler.";
            THROW_INVALID_ARGUMENT(error_builder.str());
        }

        this->name_pattern_ = name_pattern;
        this->discriminators_ = discriminators;
        this->constructor_args_ = {args...};

        std::vector<std::string> placeholders;
        for (const std::string_view& discriminator : this->discriminators_) {
            placeholders.push_back(RECORD_FIELD_TO_PLACEHOLDER_MAP.at(discriminator));
        }
        this->discriminator_pattern_ = utility::join_strings(placeholders, "-");
    }

    template<typename... Args>
    LogHandlerPtr LogRouter<Args...>::create_handler(
        const LogRecord &record,
        const std::unique_lock<std::mutex> &lock
    ) {
        if (!lock.owns_lock()) {
            THROW_LOGIC_ERROR("Cannot create a handler to route to - the router is not locked");
        }

        std::string key = this->get_handler_key(record);

        if (this->sub_handlers_.find(key) != this->sub_handlers_.end()) {
            std::ostringstream error_builder;
            error_builder << "Cannot create a handler that abides by the following discriminators: " << " There is already a handler for it";
            THROW_INVALID_ARGUMENT(error_builder.str());
        }

        if (this->handler_type_ == "FileHandler") {
            LogHandlerPtr handler = create_file_handler(
                record,
                this->name_pattern_,
                lock,
                std::forward<Args...>(this->constructor_args_)
            );
            this->sub_handlers_[key] = handler;
            return handler;
        }

        THROW_INVALID_ARGUMENT("Invalid handler type");
    }

    template<typename... Args>
    LogHandlerPtr create_file_handler(
        const LogRecord& record,
        const std::string& name_pattern,
        const std::unique_lock<std::mutex>& lock,
        Args... args
    ) {
        if (!lock.owns_lock()) {
            THROW_LOGIC_ERROR("Cannot create a FileHandler to route to - the router is not locked for mod");
        }

        std::string filename = format_log_record(record, name_pattern, "%Y%m%d_%H%M");
        if (!utility::string_type_endswith(filename, ".log")) {
            if (utility::string_type_endswith(filename, ".")) {
                filename.append(".");
            }
            filename.append(".log");
        }
        return std::make_shared<FileHandler>(filename, std::forward<Args>(args)...);
    }

    template<typename ... Args>
    std::string LogRouter<Args...>::get_handler_key(const LogRecord &record) const {
        return format_log_record(record, this->discriminator_pattern_, "%Y%m%d_%H%M");
    }

    template<typename... Args>
    LogHandlerPtr LogRouter<Args...>::get_handler(const LogRecord &record) {
        std::unique_lock<std::mutex> lock(this->sub_handlers_lock_);
        std::string key = this->get_handler_key(record);
        return this->sub_handlers_[key];
    }

    template<typename ... Args>
    LogRouter<Args...>::LogRouter(const LogRouter &other) {
        std::unique_lock this_lock(this->sub_handlers_lock_, std::defer_lock);
        std::unique_lock other_lock(other.sub_handlers_lock_, std::defer_lock);

        std::lock(this_lock, other_lock);

        this->sub_handlers_ = other.sub_handlers_;
        this->name_pattern_ = other.name_pattern_;
        this->discriminator_pattern_ = other.discriminator_pattern_;
        this->constructor_args_ = other.constructor_args_;
        this->discriminators_ = other.discriminators_;
        this->handler_type_ = other.handler_type_;
        this->maximum_buffer_size_ = other.maximum_buffer_size_;
        this->error_policy_ = other.error_policy_;
        this->error_limit_ = other.error_limit_;
        this->buffer_ = other.buffer_;
        this->enabled_ = other.enabled_;
        this->filters_ = other.filters_;
    }

    template<typename... Args>
    void LogRouter<Args...>::disable() {
        std::unique_lock<std::mutex> lock(this->sub_handlers_lock_);
        this->enabled_ = false;
        for (const auto& [key, handler] : this->sub_handlers_) {
            handler->disable();
        }
    }

    template<typename... Args>
    void LogRouter<Args...>::enable() {
        std::unique_lock<std::mutex> lock(this->sub_handlers_lock_);
        this->enabled_ = true;
        for (const auto& [key, handler] : this->sub_handlers_) {
            handler->enable();
        }
    }

    template<typename ... Args>
    LogRouter<Args...>::~LogRouter() {
        std::unique_lock<std::mutex> lock(this->sub_handlers_lock_);
        for (const auto& [key, handler] : this->sub_handlers_) {
            handler->disable();
        }

        this->sub_handlers_.clear();
    }

    template<typename ... Args>
    void LogRouter<Args...>::write(const LogRecord &record, const std::string &formatted) {
        const std::string key = this->get_handler_key(record);
        const LogHandlerPtr sub_handler = this->get_handler(record);
        sub_handler->write(record, formatted);
    }

    template<typename ... Args>
    void LogRouter<Args...>::flush() {
        std::unique_lock<std::mutex> lock(this->sub_handlers_lock_);
        for (const auto& [key, handler] : this->sub_handlers_) {
            handler->flush();
        }
    }
} // owplog::handlers