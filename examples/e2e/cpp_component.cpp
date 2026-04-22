#include "cpp_component.hpp"

#include "owplog/logger.hpp"

void cpp_component_emit_logs() {
    owplog::Logger::instance().log(
        owplog::LogLevel::Info,
        "message from native C++ component",
        "e2e.cpp",
        OWPLOG_SOURCE_LOCATION,
        owplog::LogOrigin::Application,
        -1,
        "cpp");
}
