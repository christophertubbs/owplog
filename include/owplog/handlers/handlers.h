//
// Created by christopher.tubbs on 6/1/26.
//

#ifndef OWPLOG_HANDLERS_H
#define OWPLOG_HANDLERS_H
#include <memory>

#include "console_handler.h"
#include "owplog/handlers/log_handler.h"
#include "owplog/handlers/file_handler.h"
#include "owplog/handlers/router.h"

namespace owplog::handlers {
    using handlers::LogHandlerPtr;
    using handlers::FileHandler;
    using handlers::LogRouter;
    using handlers::ConsoleHandler;
}
#endif //OWPLOG_HANDLERS_H
