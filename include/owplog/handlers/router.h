//
// Created by christopher.tubbs on 5/29/26.
//

#ifndef OWPLOG_ROUTER_H
#define OWPLOG_ROUTER_H
#include "owplog/log_record.hpp"
#include "owplog/handlers/log_handler.h"
#include "owplog/utility.hpp"

namespace owplog::handlers {
    /**
     * A type of log handler that directs log records to sub-handlers based on given fields
     * @tparam Args Positional arguments used to feed into factory functions for the sub-handlers
     */
    class LogRouter : public LogHandler {
        mutable std::mutex sub_handlers_lock_;
        std::map<std::string, LogHandlerPtr> sub_handlers_;
        std::string name_pattern_;
        std::vector<std::string_view> discriminators_;
        std::string discriminator_pattern_;

        public:
            class HandlerFactory {
                public:
                    virtual ~HandlerFactory() = default;
                    virtual LogHandlerPtr create_handler(LogRouter& router, LogRecord record) const = 0;
            };

        protected:
            std::shared_ptr<HandlerFactory> handler_factory_;

            LogHandlerPtr create_handler(
                const LogRecord& record,
                const std::unique_lock<std::mutex>& lock
            ) {
                if (!lock.owns_lock()) {
                    THROW_LOGIC_ERROR("Cannot create a handler to route to - the router is not locked");
                }

                try {
                    std::string key = this->get_handler_key(record);

                    if (this->sub_handlers_.find(key) != this->sub_handlers_.end()) {
                        std::ostringstream error_builder;
                        error_builder << "Cannot create a handler that abides by the following discriminators: " << " There is already a handler for it";
                        THROW_INVALID_ARGUMENT(error_builder.str());
                    }

                    LogHandlerPtr handler = this->handler_factory_->create_handler(*this, record);

                    handler->set_error_policy(this->error_policy_);
                    handler->set_error_limit(this->error_limit_);
                    handler->set_maximum_buffer_size(this->maximum_buffer_size_);
                    if (this->enabled_) {
                        handler->enable();
                    } else {
                        handler->disable();
                    }
                    this->filters_.for_each(
                        [&handler](const LogFilterPtr& filter) {
                            handler->add_filter(filter);
                        }
                    );

                    this->sub_handlers_[key] = handler;
                    return handler;
                } catch (const std::exception& e) {
                    std::ostringstream error_builder;
                    error_builder << "Cannot create a handler that abides by the following discriminator pattern: " << this->discriminator_pattern_ << ": " << e.what();
                    THROW_INVALID_ARGUMENT(error_builder.str());
                }
            }

            std::string get_handler_key(const LogRecord& record) const {
                return format_log_record(record, this->discriminator_pattern_, "%Y%m%d_%H%M");
            }

            LogHandlerPtr get_handler(const LogRecord& record) {
                const std::unique_lock<std::mutex> lock(this->sub_handlers_lock_);
                const std::string key = this->get_handler_key(record);

                auto matching_handler = this->sub_handlers_.find(key);

                if (matching_handler == this->sub_handlers_.end()) {
                    LogHandlerPtr handler = this->create_handler(record, lock);
                    this->sub_handlers_[key] = handler;
                    return handler;
                }
                return matching_handler->second;
            }

        public:
            LogRouter(
                std::string name_pattern,
                std::vector<std::string_view> discriminators,
                std::shared_ptr<HandlerFactory> handler_factory
            ) {
                this->name_pattern_ = name_pattern;
                this->discriminators_ = discriminators;
                this->handler_factory_ = handler_factory;

                std::vector<std::string> placeholders;
                for (const std::string_view& discriminator : this->discriminators_) {
                    try {
                        placeholders.push_back(RECORD_FIELD_TO_PLACEHOLDER_MAP.at(discriminator));
                    } catch (const std::out_of_range&) {
                        THROW_OUT_OF_RANGE("Cannot create a handler that abides by the following discriminator: " + std::string(discriminator));
                    }
                }
                this->discriminator_pattern_ = utility::join_strings(placeholders, "-");
            }

            LogRouter(const LogRouter& other) {
                std::unique_lock this_lock(this->sub_handlers_lock_, std::defer_lock);
                std::unique_lock other_lock(other.sub_handlers_lock_, std::defer_lock);

                std::lock(this_lock, other_lock);

                this->sub_handlers_ = other.sub_handlers_;
                this->name_pattern_ = other.name_pattern_;
                this->discriminator_pattern_ = other.discriminator_pattern_;
                this->discriminators_ = other.discriminators_;
                this->maximum_buffer_size_ = other.maximum_buffer_size_;
                this->error_policy_ = other.error_policy_;
                this->error_limit_ = other.error_limit_;
                this->buffer_ = other.buffer_;
                this->enabled_ = other.enabled_;
                this->filters_ = other.filters_;
                this->handler_factory_ = other.handler_factory_;
            }

            LogRouter() = delete;
            ~LogRouter() override {
                std::unique_lock<std::mutex> lock(this->sub_handlers_lock_);
                for (const auto& [key, handler] : this->sub_handlers_) {
                    handler->disable();
                }

                this->sub_handlers_.clear();
            }

            void write(const LogRecord& record, const std::string& formatted) override {
                const std::string key = this->get_handler_key(record);
                const LogHandlerPtr sub_handler = this->get_handler(record);
                sub_handler->write(record, formatted);
            }

            void flush() override {
                std::unique_lock<std::mutex> lock(this->sub_handlers_lock_);
                for (const auto& [key, handler] : this->sub_handlers_) {
                    handler->flush();
                }
            }

            std::string get_name_pattern() const {
                return this->name_pattern_;
            }

            void enable() override {
                std::unique_lock<std::mutex> lock(this->sub_handlers_lock_);
                this->enabled_ = true;
                for (const auto& [key, handler] : this->sub_handlers_) {
                    handler->enable();
                }
            }

            void disable() override {
                std::unique_lock<std::mutex> lock(this->sub_handlers_lock_);
                this->enabled_ = false;
                for (const auto& [key, handler] : this->sub_handlers_) {
                    handler->disable();
                }
            }

            void set_error_limit(std::size_t limit) override {
                const std::lock_guard<std::mutex> lock(this->sub_handlers_lock_);
                this->error_limit_ = limit;
                for (auto& [key, handler] : this->sub_handlers_) {
                    handler->set_error_limit(limit);
                }
            }

            void set_error_policy(ErrorPolicy policy) override {
                const std::lock_guard<std::mutex> lock(this->sub_handlers_lock_);
                this->error_policy_ = policy;
                for (auto& [key, handler] : this->sub_handlers_) {
                    handler->set_error_policy(policy);
                }
            };

            void set_maximum_buffer_size(std::size_t size) override {
                const std::lock_guard<std::mutex> lock(this->sub_handlers_lock_);
                this->maximum_buffer_size_ = size;
                for (auto& [key, handler] : this->sub_handlers_) {
                    handler->set_maximum_buffer_size(size);
                }
            }

            void add_filter(LogFilterPtr filter) override {
                const std::lock_guard<std::mutex> lock(this->sub_handlers_lock_);
                this->filters_.push_back(filter);
                for (auto& [key, handler] : this->sub_handlers_) {
                    handler->add_filter(filter);
                }
            }
    };

    class FileHandlerFactory : public LogRouter::HandlerFactory {
        std::filesystem::path base_path_;
        bool append_ = false;

        public:
            FileHandlerFactory(const std::filesystem::path& base_path, const bool append) : base_path_(base_path), append_(append) {}
            FileHandlerFactory(const std::string_view base_path, const bool append) : base_path_(base_path), append_(append) {}
            FileHandlerFactory(const FileHandlerFactory& other) {
                this->base_path_ = other.base_path_;
                this->append_ = other.append_;
            }
            LogHandlerPtr create_handler(LogRouter& router, LogRecord record) const override {
                const std::string filename = format_log_record(record, router.get_name_pattern(), "%Y%m%d_%H%M");
                const std::filesystem::path log_path = this->base_path_ / filename;
                return std::make_shared<FileHandler>(log_path, this->append_);
            }
    };
} // owplog::handlers

#endif //OWPLOG_ROUTER_H
