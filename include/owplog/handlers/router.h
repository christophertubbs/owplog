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
    template<typename... Args>
    class LogRouter : public LogHandler {
        std::initializer_list<Args...> constructor_args_;
        std::string_view handler_type_;

        protected:
            std::mutex sub_handlers_lock_;
            std::map<std::string, LogHandlerPtr> sub_handlers_;
            std::string name_pattern_;
            std::vector<std::string_view> discriminators_;
            std::string discriminator_pattern_;
            LogHandlerPtr create_handler(
                const LogRecord& record,
                const std::unique_lock<std::mutex>& lock
            );
            std::string get_handler_key(const LogRecord& record) const;
            LogHandlerPtr get_handler(const LogRecord& record);

        public:
            LogRouter(
                const std::string_view& name_pattern,
                const std::string_view& handler_type,
                std::vector<std::string_view>& discriminators,
                Args... args
            );
            LogRouter(const LogRouter& other);
            LogRouter() = delete;
            ~LogRouter() override;

            void write(const LogRecord& record, const std::string& formatted) override;
            void flush() override;
            void enable() override;
            void disable() override;
            void set_error_limit(std::size_t limit) override;
            void set_error_policy(ErrorPolicy policy) override;
            void set_maximum_buffer_size(std::size_t size) override;
            void add_filter(LogFilterPtr filter) override;;
    };
} // owplog::handlers

#endif //OWPLOG_ROUTER_H
