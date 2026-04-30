#include "c_component.h"
#include "cpp_component.hpp"

#include "owplog/logger.hpp"

#include <iostream>

#if defined(OWPLOG_E2E_HAVE_FORTRAN)
extern "C" void fortran_component_emit_logs();
#endif

#if defined(OWPLOG_E2E_HAVE_PYTHON_EMBED)
#include <pybind11/embed.h>
namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(owplog_native_bridge, m) {
    m.def(
        "emit",
        [](const std::string& logger_name,
           const std::string& message,
           const std::string& file_name,
           int line_no,
           const std::string& function_name,
           int rank) {
            owplog::SourceLocation source{file_name, line_no, function_name};
            owplog::Logger::instance().log(
                owplog::LogLevel::Info,
                message,
                logger_name,
                source,
                owplog::LogOrigin::Application,
                rank,
                "python");
        });
}
#endif

int main() {
    auto& logger = owplog::Logger::instance();
    logger.clear_sinks();
    logger.clear_filters();
    logger.set_level(owplog::LogLevel::Trace);
    logger.set_formatter(std::make_shared<owplog::PatternFormatter>("%T [%L] (%N) %f:%l %F rank=%r lang=%g %m"));
    //logger.configure_default_console();
    logger.add_file_sink("owplog_e2e_native.log", false);
    logger.start_stdio_capture();

    logger.log(owplog::LogLevel::Info, "main started and logger configured", "e2e.main", OWPLOG_SOURCE_LOCATION);
    cpp_component_emit_logs();
    cpp_echo_messages();
    c_component_emit_logs();
#if defined(OWPLOG_E2E_HAVE_FORTRAN)
    fortran_component_emit_logs();
#else
    std::cout << "[INFO] Fortran compiler not detected; skipping Fortran component.\n";
#endif

#if defined(OWPLOG_E2E_HAVE_PYTHON_EMBED)
    py::scoped_interpreter guard{};
    py::exec(R"PY(
import inspect
import owplog_native_bridge

def python_component():
    f = inspect.currentframe()
    owplog_native_bridge.emit(
        "e2e.python",
        "message from Python embedded in native e2e",
        f.f_code.co_filename,
        f.f_lineno,
        f.f_code.co_name,
        -1,
    )

python_component()
)PY");
#else
    std::cout << "[INFO] Python embed dependencies not detected; skipping Python component.\n";
#endif

    logger.log(owplog::LogLevel::Info, "main finished", "e2e.main", OWPLOG_SOURCE_LOCATION);
    logger.flush();
    return 0;
}
