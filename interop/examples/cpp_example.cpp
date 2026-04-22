#include "owplog_cxx_logger.hpp"

/**
 * @brief Minimal C++ example using RAII C ABI wrapper.
 * @return Process exit code.
 */
int main() {
    // Wrapper object for C ABI message submission.
    owplog::interop::CApiLogger logger;
    // Register named sublogger for this component.
    const owplog_sublogger_handle sublogger = logger.add_sublogger("interop.cpp");
    if (sublogger == 0) {
        return 1;
    }

    // Submit both direct-name and handle-based messages.
    logger.log(OWPLOG_LOG_INFO, "interop.cpp", "hello from C++ helper");
    logger.log_sublogger(OWPLOG_LOG_INFO, sublogger, "hello from C++ sublogger handle");
    const owplog_log_meta meta{__FILE__, __LINE__, __func__, "cpp", -1};
    logger.log_ex(OWPLOG_LOG_INFO, "interop.cpp", "hello with metadata", meta);
    return 0;
}
