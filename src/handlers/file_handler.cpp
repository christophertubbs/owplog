//
// Created by christopher.tubbs on 5/28/26.
//
#include "owplog/handlers/file_handler.h"
namespace owplog::handlers {

    /**
     * @brief Construct file handler stream.
     * @param path Log output path.
     * @param append Append mode when true, truncate when false.
     */
    FileHandler::FileHandler(const std::filesystem::path& path, const bool append) {
        // Stream mode selected from append/truncate behavior.
        if (!std::filesystem::exists(path.parent_path())) {
            throw std::filesystem::filesystem_error(
                path.parent_path().string() + " does not exist - cannot create a log file there",
                path.parent_path(),
                make_error_code(std::errc::no_such_file_or_directory)
            );
        }
        const auto mode = append ? (std::ios::out | std::ios::app) : (std::ios::out | std::ios::trunc);
        this->stream_.open(path, mode);
        if (!this->stream_) {
            throw std::runtime_error(std::string("Unable to open log file: ") + path.string());
        }
    }

    /**
     * @brief Write one formatted line to file handler.
     * @param formatted Formatted record text.
     */
    void FileHandler::write(const LogRecord&, const std::string& formatted) {
        std::lock_guard lock(this->file_mutex_);
        this->buffer_.append(formatted).append("\n");
        if (this->buffer_.size() >= this->maximum_buffer_size_) {
            this->flush_file();
        }
    }

    /**
     * @brief Flush file handler stream.
     */
    void FileHandler::flush() {
        std::lock_guard lock(this->file_mutex_);
        this->flush_file();
    }

    void FileHandler::flush_file() {
        this->stream_ << this->buffer_;
        this->buffer_.clear();
        this->stream_.flush();
    }
}