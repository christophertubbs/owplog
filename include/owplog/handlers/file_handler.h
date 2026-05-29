//
// Created by christopher.tubbs on 5/28/26.
//

#ifndef OWPLOG_FILE_HANDLER_H
#define OWPLOG_FILE_HANDLER_H
#include <filesystem>
#include <fstream>
#include "owplog/handlers/log_handler.h"
namespace owplog::handlers {

    /**
     * @brief Handler that writes to a file stream.
     */
    class FileHandler final : public LogHandler {
        public:
            /**
             * @brief Construct file handler.
             * @param path Destination file path.
             * @param append True to append, false to truncate.
             * @pre `path` must be writable by current process.
             * @post On success, file stream is open.
             * @throws `std::runtime_error` when file open fails.
             */
            explicit FileHandler(const std::string& path, const bool append = true) : FileHandler(std::filesystem::path(path), append) {}
            explicit FileHandler(const std::filesystem::path& path, const bool append = true);
            explicit FileHandler(const char* path, const bool append = true) : FileHandler(std::filesystem::path(path), append) {};
            FileHandler(
                const std::string_view& filename,
                const std::string_view& path,
                const bool append = true
            ) : FileHandler(std::filesystem::path(path) / filename, append){}
            FileHandler(
                const std::string_view& filename,
                const std::filesystem::path& path,
                const bool append = true
            ) : FileHandler(path / filename, append) {}
            /** @copydoc LogHandler::write */
            void write(const LogRecord& record, const std::string& formatted) override;
            /** @copydoc LogHandler::flush */
            void flush() override;

        private:
            void flush_file();
            /** Mutex protecting file stream writes and flushes. */
            std::mutex file_mutex_;
            /** Output stream backing this handler. */
            std::ofstream stream_;
    };
}
#endif //OWPLOG_FILE_HANDLER_H
