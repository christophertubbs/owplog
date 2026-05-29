//
// Created by christopher.tubbs on 5/28/26.
//

#ifndef OWPLOG_CONSOLE_HANDLER_H
#define OWPLOG_CONSOLE_HANDLER_H
#include <unistd.h>
#include "owplog/handlers/log_handler.h"

namespace owplog::handlers {
    /**
     * @brief Handler that writes to stdout/stderr.
     */
    class ConsoleHandler final : public LogHandler {
        public:
            /**
             * @brief Construct console handler behavior.
             * @param stderr_for_errors Route error and above to stderr.
             * @param suppress_captured_streams Avoid echo-looping captured stdio.
             * @pre None.
             * @post Console handler configuration is fixed for instance lifetime.
             * @throws No intentional exceptions.
             */
            ConsoleHandler(bool stderr_for_errors = true, bool suppress_captured_streams = true);
            /** @copydoc LogHandler::write */
            void write(const LogRecord& record, const std::string& formatted) override;
            void set_stderr_descriptor(int descriptor);
            void set_stdout_descriptor(int descriptor);

        private:
            /** Whether error-level records should go to stderr. */
            bool stderr_for_errors_;
            /** Whether captured stdout/stderr records should be suppressed on console output. */
            bool suppress_captured_streams_;
            /** Mutex protecting console stream writes and flushes. */
            std::mutex io_mutex_;
            std::atomic<std::size_t> stdout_descriptor_{STDOUT_FILENO};
            std::atomic<std::size_t> stderr_descriptor_{STDERR_FILENO};
    };
}
#endif //OWPLOG_CONSOLE_HANDLER_H
