#include "c_component.h"

#include "owplog/c_api.h"

void c_component_emit_logs(void) {
    owplog_logger_log(OWPLOG_LOG_INFO, "e2e.c", "message from C component");

    const owplog_sublogger_handle h = owplog_logger_add_sublogger("e2e.c.sublogger");
    if (h > 0) {
        owplog_logger_log_sublogger(OWPLOG_LOG_WARNING, h, "warning via C sublogger handle");
    }

    const owplog_log_meta meta = {__FILE__, __LINE__, __func__, "c", -1};
    owplog_logger_log_ex(OWPLOG_LOG_INFO, "e2e.c", "info with C source metadata", &meta);
}
