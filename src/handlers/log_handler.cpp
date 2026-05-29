//
// Created by christopher.tubbs on 5/28/26.
//
#include "owplog/handlers/log_handler.h"
namespace owplog::handlers {

    /**
     * @brief Add handler-local filter.
     * @param filter Filter to append.
     */
    void LogHandler::add_filter(LogFilterPtr filter) {
        this->filters_.push_back(std::move(filter));
    }

    /**
     * @brief Remove all handler-local filters.
     */
    void LogHandler::clear_filters() {
        this->filters_.clear();
    }

    /**
     * @brief Evaluate handler-local filters for a record.
     * @param record Record under evaluation.
     * @return True when all local filters allow record.
     */
    bool LogHandler::should_log(const LogRecord& record) const {
        return this->is_enabled() && this->filters_.all_of(
            [&record](const auto& filter) {
                return !filter || filter->allow(record);
            }
        );
    }

    void LogHandler::set_maximum_buffer_size(const std::size_t size) {
        if (size == 0) {
            throw std::invalid_argument("Maximum buffer size cannot be zero");
        }
        this->maximum_buffer_size_ = size;
    }

    void LogHandler::set_error_policy(ErrorPolicy policy) {
        this->error_policy_ = policy;
    }

    /**
     * @brief Default no-op flush implementation.
     */
    void LogHandler::flush() {}

    bool LogHandler::is_enabled() const {
        return this->enabled_;
    }

    void LogHandler::enable() {
        this->enabled_ = true;
    }

    void LogHandler::disable() {
        this->enabled_ = false;
    }

    void LogHandler::set_error_limit(std::size_t limit) {
        this->error_limit_ = limit;
    }

    void LogHandler::reset_error_count() {
        this->error_count_ = 0;
    }

    void LogHandler::handle_error(const std::string &message) {
        switch (this->error_policy_) {
            case ErrorPolicy::Count:
                ++this->error_count_;
                if (this->error_count_ >= this->error_limit_) {
                    throw std::runtime_error(message);
                }
                break;
            case ErrorPolicy::Throw:
                throw std::runtime_error(message);
            case ErrorPolicy::Disable:
                this->disable();
                break;

            case ErrorPolicy::Ignore:
            default:
                break;
        }
    }
}