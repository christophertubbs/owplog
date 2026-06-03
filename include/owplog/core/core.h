//
// Created by christopher.tubbs on 6/1/26.
//

#ifndef OWPLOG_CORE_H
#define OWPLOG_CORE_H
#include "owplog/core/logger.h"

namespace owplog::core {
    using core::Logger;
    using core::LoggerConfigurationArguments;
    using core::shutdown;
    using core::set_level;
    using core::level;
    using core::set_formatter;
    using core::add_handler;
    using core::clear_handlers;
    using core::clear_filters;
    using core::add_filter;
    using core::has_handlers;
    using core::is_configured;
    using core::log;
    using core::error;
    using core::warning;
    using core::info;
    using core::debug;
    using core::critical;
    using core::flush;
    using core::configure;
}
#endif //OWPLOG_CORE_H
