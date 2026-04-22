#include "owplog_logger_client.h"

/**
 * @brief Minimal C example using convenience interop wrapper.
 * @return Process exit code.
 */
int main(void) {
    /* Register a named sublogger for handle-based logging. */
    const owplog_sublogger_handle sublogger = owplog_logger_client_add_sublogger("interop.c");
    if (sublogger == 0) {
        return 1;
    }

    /* Emit both direct-name and handle-based messages. */
    owplog_logger_client_log_info("interop.c", "hello from C helper");
    owplog_logger_client_log_sublogger_info(sublogger, "hello from C sublogger handle");
    OWPLOG_LOG_INFO_EX_C("interop.c", "hello with source metadata", "c", -1);
    return 0;
}
