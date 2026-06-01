//
// Created by christopher.tubbs on 6/1/26.
//
#include "owplog/core/logger.h"

#include <unistd.h>

/** POSIX wrapper for duplicating file descriptors. */
inline int duplicate_file_descriptor(const int file_descriptor) { return dup(file_descriptor); }
/** POSIX wrapper for replacing a file descriptor. */
inline int redirect_file_descriptor(const int from, const int to) { return dup2(from, to); }
/** POSIX wrapper for closing file descriptors. */
inline int close_file_descriptor(const int file_descriptor) { return close(file_descriptor); }
/** POSIX wrapper for reading bytes from file descriptors. */
inline int read_descriptor_into_buffer(const int file_descriptor, void* buffer, const unsigned int count) { return static_cast<int>(read(file_descriptor, buffer, count)); }
/** POSIX wrapper for creating anonymous pipes. */
inline int create_pipe(int* fds) { return pipe(fds); }
/** POSIX wrapper for retrieving FILE* descriptor. */
inline int get_stream_file_descriptor(FILE* stream) { return fileno(stream); }

namespace owplog::core {
    /** Ensure capture is stopped during destruction. */
    LoggerStdioCapture::~LoggerStdioCapture() {
        this->stop();
    }

    /**
     * @brief Start capture threads for selected streams.
     * @param capture_stdout Whether stdout should be captured.
     * @param capture_stderr Whether stderr should be captured.
     * @return True on success.
     */
    bool LoggerStdioCapture::start(const bool capture_stdout, const bool capture_stderr) {
        if (this->running_) {
            return false;
        }

        // stdout capture success indicator (or trivially true when disabled).
        const bool stdout_ok = !capture_stdout || listen_to_stream(stdout_state_, stdout, LogOrigin::CapturedStdout, "stdout");
        // stderr capture success indicator (or trivially true when disabled).
        const bool stderr_ok = !capture_stderr || listen_to_stream(stderr_state_, stderr, LogOrigin::CapturedStderr, "stderr");
        if (!stdout_ok || !stderr_ok) {
            stop();
            return false;
        }

        running_ = true;
        return true;
    }

    /**
     * @brief Stop capture threads and restore original descriptors.
     */
    void LoggerStdioCapture::stop() {
        if (!running_) {
            return;
        }
        stop_listening(stdout_state_);
        stop_listening(stderr_state_);
        running_ = false;
    }
    /**
     * @brief Start redirection for one stream.
     * @param state Mutable stream state slot.
     * @param stream C stdio stream pointer.
     * @param origin Origin metadata for forwarded records.
     * @param label Logger name for forwarded records.
     * @return True on success.
     */
    bool LoggerStdioCapture::listen_to_stream(StreamState& state, FILE* stream, const LogOrigin origin, const std::string& label) const {
        if (state.active) {
            return false;
        }

        // File descriptor associated with FILE* stream.
        const int target_fd = get_stream_file_descriptor(stream);
        if (target_fd < 0) {
            return false;
        }

        std::fflush(stream);
        // Saved copy used to restore the original stream on stop.
        const int original_fd = duplicate_file_descriptor(target_fd);
        if (original_fd < 0) {
            return false;
        }

        // Pipe descriptors used for stream redirection.
        int pipe_fds[2] = {-1, -1};
        int pipe_created = create_pipe(pipe_fds);
        if (pipe_created != 0) {
            fprintf(stderr, "Pipes couldn't be created. Closing the original file descriptor: %d\n", original_fd);
            close_file_descriptor(original_fd);
            return false;
        }

        int second_pipe_redirected = redirect_file_descriptor(pipe_fds[1], target_fd);
        // at this point nothing is really touching the given stream. If it's stderr, whoopsies, no more communication
        if (second_pipe_redirected < 0) {
            // If you're here, it is because something broke
            close_file_descriptor(original_fd);
            close_file_descriptor(pipe_fds[0]);
            close_file_descriptor(pipe_fds[1]);
            return false;
        }

        // Parent process only needs read side after descriptor replacement.
        close_file_descriptor(pipe_fds[1]);

        state.active = true;
        state.stream_fd = original_fd;
        state.read_fd = pipe_fds[0];
        state.target_fd = target_fd;
        state.origin = origin;
        state.label = label;
        // Reader thread drains pipe and forwards line-delimited records.
        state.reader = std::thread([this, &state]() {
            // Buffered partial line bytes that span read chunks.
            std::string pending;
            // Scratch buffer used for fd reads.
            char buffer[256];
            size_t buffer_size = sizeof(buffer);
            while (true) {
                // Number of bytes read from pipe.
                const int bytes = read_descriptor_into_buffer(state.read_fd, buffer, buffer_size);
                if (bytes <= 0) {
                    break;
                }

                write(state.stream_fd, buffer, static_cast<std::size_t>(bytes));
                pending.append(buffer, static_cast<std::size_t>(bytes));
                // Scan pending buffer for newline-delimited complete lines.
                std::size_t pos = 0;
                while (true) {
                    // Index of next newline in pending buffer.
                    const std::size_t newline = pending.find('\n', pos);
                    if (newline == std::string::npos) {
                        pending.erase(0, pos);
                        break;
                    }
                    // Completed line text excluding delimiter.
                    const std::string line = pending.substr(pos, newline - pos);
                    if (!line.empty()) {
                        this->logger_.log(LogLevel::Info, line, state.label, {}, state.origin);
                    }
                    pos = newline + 1;
                }
            }

            // Flush trailing non-newline text as final record.
            if (!pending.empty()) {
                LogLevel stateLevel = state.origin == LogOrigin::CapturedStdout ? LogLevel::Info : LogLevel::Error;
                this->logger_.log(stateLevel, pending, state.label, {}, state.origin);
            }
            close_file_descriptor(state.read_fd);
            state.read_fd = -1;
        });

        return true;
    }


    /**
     * @brief Stop one stream redirection and restore original descriptor.
     * @param state Mutable stream state slot.
     */
    void LoggerStdioCapture::stop_listening(StreamState& state) {
        if (!state.active) {
            return;
        }

        // Flush redirected stream before restoring original descriptor.
        std::fflush(state.target_fd == get_stream_file_descriptor(stdout) ? stdout : stderr);
        redirect_file_descriptor(state.stream_fd, state.target_fd);

        if (state.reader.joinable()) {
            state.reader.join();
        }

        close_file_descriptor(state.stream_fd);
        state.stream_fd = -1;
        state.active = false;
    }
}