#include "cpp_component.hpp"
#include "printf.h"
#include <cstdio>

#include "owplog/c_api.h"

void cpp_component_emit_logs() {
    LOG_NAMED_INFO("cpp_component.cpp", "message from native C++ component");
}


void cpp_echo_messages() {
    std::fprintf(
        stderr,
        "Here is an error from %s that wasn't sent to a logger on line %d\n",
        __FILE__,
        __LINE__
    );
}