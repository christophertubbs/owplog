#include "owplog/c_api.h"
#include "owplog/logger.hpp"

#include <pybind11/embed.h>

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(owplog_pybridge, m) {
    m.def(
        "emit",
        [](const std::string& logger_name,
           const std::string& message,
           const std::string& file_name,
           int line_no,
           const std::string& function_name,
           int rank) {
            const owplog_log_meta meta{
                file_name.c_str(),
                line_no,
                function_name.c_str(),
                "python",
                rank};
            owplog_logger_log_ex(OWPLOG_LOG_INFO, logger_name.c_str(), message.c_str(), &meta);
        });
}

int main() {
    auto& logger = owplog::Logger::instance();
    logger.clear_handlers();
    logger.clear_filters();
    logger.set_level(owplog::LogLevel::Trace);
    logger.set_formatter(std::make_shared<owplog::PatternFormatter>("%T [%L] (%N) %f:%l %F rank=%r lang=%g %m"));
    logger.configure_default_console();
    logger.add_file_sink("owplog_e2e_pybind11.log", false);

    py::scoped_interpreter guard{};
    py::exec(R"PY(
import inspect
import owplog_pybridge

def python_component():
    f = inspect.currentframe()
    owplog_pybridge.emit(
        "e2e.python",
        "message from Python via pybind11 embedded module",
        f.f_code.co_filename,
        f.f_lineno,
        f.f_code.co_name,
        -1,
    )

python_component()
)PY");

    logger.flush();
    return 0;
}
