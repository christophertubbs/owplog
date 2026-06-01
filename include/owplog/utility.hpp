//
// Created by christopher.tubbs on 5/11/26.
//

#ifndef OWPLOG_UTILITY_H
#define OWPLOG_UTILITY_H

#include <shared_mutex>
#include <type_traits>
#include <vector>
#include <optional>
#include <algorithm>
#include <condition_variable>
#include <utility>
#include <mutex>
#include <cstddef>
#include <deque>
#include <functional>
#include <regex>
#include <stdexcept>
#include <string_view>
#include <chrono>
#include <unistd.h>

namespace owplog::utility {
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    const TimePoint INITIALIZATION_TIME = Clock::now();

    template<typename ClockType>
    [[nodiscard]] std::string format_duration(
        const std::chrono::time_point<ClockType>& first,
        const std::chrono::time_point<ClockType>& second,
        const bool absolute = true
    ) {
        auto elapsed_duration = second - first;
        bool was_negative = elapsed_duration < elapsed_duration.zero();
        if (was_negative) {
            elapsed_duration = -elapsed_duration;
        }

        const auto total_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_duration).count();

        constexpr long long milliseconds_per_second = 1000;
        constexpr long long seconds_per_minute = 60;
        constexpr long long minutes_per_hour = 60;

        const long long total_seconds = total_milliseconds / milliseconds_per_second;
        const long long millisecond_component = total_milliseconds % milliseconds_per_second;

        const long long second_component = total_seconds % seconds_per_minute;
        const long long total_minutes = total_seconds / seconds_per_minute;

        const long long minute_component = total_minutes % minutes_per_hour;
        const long long hour_component = total_minutes / minutes_per_hour;

        std::ostringstream output_stream;
        if (!absolute && was_negative) {
            output_stream << "-";
        }

        output_stream << std::setw(2) << std::setfill('0') << hour_component;
        output_stream << ":";
        output_stream << std::setw(2) << std::setfill('0') << minute_component;
        output_stream << ":";
        output_stream << std::setw(2) << std::setfill('0') << second_component;
        output_stream << ".";
        output_stream << std::setw(3) << std::setfill('0') << millisecond_component;

        return output_stream.str();
    }

    inline bool string_type_endswith(const std::string_view text, const std::string_view suffix) {
        if (suffix.size() > text.size()) {
            return false;
        }

        const std::size_t suffix_start_index = text.size() - suffix.size();
        return text.compare(suffix_start_index, suffix.size(), suffix) == 0;
    }

    inline std::string join_strings(const std::vector<std::string>& strings, const std::string& separator) {
        std::ostringstream output_stream;

        for (std::size_t string_index = 0; string_index < strings.size(); ++string_index) {
            if (string_index > 0) {
                output_stream << separator;
            }
            output_stream << strings[string_index];
        }

        return output_stream.str();
    }

    template<typename ValueType>
    class DispatchQueue {
        std::deque<ValueType> queue_;
        mutable std::mutex queue_lock_;
        std::condition_variable insertion_signal_;
        int in_flight_{0};
        bool shutdown_requested_{false};

        public:
            DispatchQueue() = default;
            ~DispatchQueue() = default;
            DispatchQueue(const DispatchQueue& other) = delete;
            DispatchQueue& operator=(const DispatchQueue& other) = delete;

            DispatchQueue(DispatchQueue&&) = delete;
            DispatchQueue& operator=(DispatchQueue&&) = delete;

            std::optional<ValueType> next() {
                std::unique_lock lock(this->queue_lock_);
                if (this->queue_.empty()) {
                    return std::nullopt;
                }
                ValueType value = std::move(this->queue_.front());
                this->queue_.pop_front();
                ++this->in_flight_;
                return value;
            }

            bool submit(ValueType value) {
                {
                    std::unique_lock lock(this->queue_lock_);

                    if (this->shutdown_requested_) {
                        return false;
                    }

                    this->queue_.push_back(std::move(value));
                }
                this->insertion_signal_.notify_one();
                return true;
            }

            void mark_completed() {
                std::unique_lock lock(this->queue_lock_);
                if (this->in_flight_ > 0) {
                    --this->in_flight_;
                } else {
                    throw std::runtime_error("Cannot mark completed when no items are in flight");
                }
            }

            std::optional<ValueType> listen_for_submission() {
                std::unique_lock lock(this->queue_lock_);
                this->insertion_signal_.wait(
                    lock,
                    [this] {
                        return !this->queue_.empty() || this->shutdown_requested_;
                    }
                    );
                if (this->queue_.empty()) {
                    return std::nullopt;
                }
                ValueType value = std::move(this->queue_.front());
                this->queue_.pop_front();
                ++this->in_flight_;
                return value;
            }

            void request_shutdown() {
                {
                    std::lock_guard lock(this->queue_lock_);
                    this->shutdown_requested_ = true;
                }
                this->insertion_signal_.notify_all();
            }

            bool accepting_submissions() const {
                std::lock_guard lock(this->queue_lock_);
                return !this->shutdown_requested_;
            }
    };

    template<typename ValueType>
    class TrackedDispatch {
        DispatchQueue<ValueType> *queue_;
        ValueType value_;

        public:
            TrackedDispatch(ValueType value, DispatchQueue<ValueType>* queue) : queue_{queue}, value_(std::move(value)) {}
            TrackedDispatch(TrackedDispatch&& other) noexcept : queue_(other.queue_), value_(std::move(other.value_)) {
                // The other queue_ has to be set to null or else the completion will be marke
                other.queue_ = nullptr;
            }
            TrackedDispatch& operator=(TrackedDispatch&& other) noexcept {
                if (this == &other) {
                    return *this;
                }

                if (this->queue_ != nullptr) {
                    this->queue_->mark_completed();
                }
                queue_ = other.queue_;
                value_ = std::move(other.value_);
                return *this;
            }

            ~TrackedDispatch() {
                if (queue_ != nullptr) {
                    queue_->mark_completed();
                }
            }

            ValueType& get() {
                return this->value_;
            }

            const ValueType& get() const {
                return this->value_;
            }
    };

    template<typename ValueType>
    class TrackedDispatchQueue {
        DispatchQueue<ValueType> queue_;
        public:
            std::optional<TrackedDispatch<ValueType>> next() {
                std::optional<ValueType> next_value = this->queue_.next();

                if (!next_value.has_value()) {
                    return std::nullopt;
                }

                return TrackedDispatch<ValueType>(std::move(*next_value), &this->queue_);
            }

            std::optional<TrackedDispatch<ValueType>> listen_for_submission() {
                std::optional<ValueType> next_value = this->queue_.listen_for_submission();

                if (!next_value.has_value()) {
                    return std::nullopt;
                }

                return TrackedDispatch<ValueType>(std::move(*next_value), &this->queue_);
            }

            [[nodiscard]] bool accepting_submissions() const {
                return this->queue_.accepting_submissions();
            }

            bool submit(ValueType value) {
                return this->queue_.submit(std::move(value));
            }

            void request_shutdown() {
                this->queue_.request_shutdown();
            }
    };

    template<typename ValueType>
    class ProtectedVector {
        public:
            ProtectedVector() = default;
            ~ProtectedVector() = default;
            explicit ProtectedVector(std::initializer_list<ValueType> initial_data) : data_(initial_data) {}
            explicit ProtectedVector(std::vector<ValueType> initial_data) : data_(std::move(initial_data)) {}
            explicit ProtectedVector(const ValueType* initial_data, size_t size) : data_(initial_data, initial_data + size) {}
            ProtectedVector(const ProtectedVector& other) {
                std::shared_lock<std::shared_mutex> source_lock(other.lock_);
                this->data_ = other.data_;
            }
            ProtectedVector(ProtectedVector&& other) noexcept {
                std::unique_lock<std::shared_mutex> source_lock(other.lock_);
                this->data_ = std::move(other.data_);
            }

            ProtectedVector& operator=(const ProtectedVector& other) {
                if (this == &other) {
                    return *this;
                }
                std::unique_lock<std::shared_mutex> destination_lock(this->lock_, std::defer_lock);
                std::shared_lock<std::shared_mutex> source_lock(other.lock_, std::defer_lock);
                std::lock(destination_lock, source_lock);
                this->data_ = other.data_;
                return *this;
            }

            ProtectedVector& operator=(ProtectedVector&& other) noexcept {
                if (this == &other) {
                    return *this;
                }
                std::unique_lock<std::shared_mutex> destination_lock(this->lock_, std::defer_lock);
                std::unique_lock<std::shared_mutex> source_lock(other.lock_, std::defer_lock);
                std::lock(destination_lock, source_lock);
                this->data_ = std::move(other.data_);
                return *this;
            }

            std::vector<ValueType> snapshot() const {
                std::shared_lock<std::shared_mutex> lock(this->lock_);
                return this->data_;
            }

            template<typename Operator, class... Args>
            auto on_snapshot_range(Operator function, Args&&... args) const {
                std::vector<ValueType> snapshot;
                {
                    std::shared_lock<std::shared_mutex> lock(this->lock_);
                    snapshot = this->data_;
                }

                return function(snapshot.begin(), snapshot.end(), std::forward<Args>(args)...);
            }

            template<typename Predicate>
            bool all_of(Predicate predicate) const {
                std::vector<ValueType> snapshot;
                {
                    std::shared_lock<std::shared_mutex> lock(this->lock_);
                    snapshot = this->data_;
                }
                return std::all_of(snapshot.begin(), snapshot.end(), predicate);
            }

            template<typename Predicate>
            bool any_of(Predicate predicate) const {
                std::vector<ValueType> snapshot;
                {
                    std::shared_lock<std::shared_mutex> lock(this->lock_);
                    snapshot = this->data_;
                }
                return std::any_of(snapshot.begin(), this->data_.end(), predicate);
            }

            template<typename Predicate>
            bool none_of(Predicate predicate) const {
                std::vector<ValueType> snapshot;
                {
                    std::shared_lock<std::shared_mutex> lock(this->lock_);
                    snapshot = this->data_;
                }
                return std::none_of(snapshot.begin(), snapshot.end(), predicate);
            }

            template<typename Transformer>
            auto on_index(size_t index, Transformer callback) const {
                ValueType copy = [&] {
                    std::shared_lock<std::shared_mutex> lock(this->lock_);
                    return this->data_.at(index);
                }();

                return callback(copy);
            };

            template<typename Transformer, class... Args>
            auto on_index(size_t index, Transformer callback, Args&&... args) const {
                ValueType copy = [&] {
                    std::shared_lock<std::shared_mutex> lock(this->lock_);
                    return this->data_.at(index);
                }();

                return callback(copy, std::forward<Args>(args)...);
            };

            template<typename Selector, typename Callback>
            void on_selected(Selector selector, Callback callback) const {
                std::shared_lock<std::shared_mutex> lock(this->lock_);
                for (const auto& item : this->data_) {
                    if (selector(item)) {
                        callback(item);
                    }
                }
            }

            template<typename Selector, typename Callback, class... Args>
            void on_selected(Selector selector, Callback callback, Args&&... args) const {
                std::unique_lock<std::shared_mutex> lock(this->lock_);
                for (const auto& item : this->data_) {
                    if (selector(item, std::forward<Args>(args)...)) {
                        callback(item);
                    }
                }
            }

            template<typename Transformer>
            bool on_front(Transformer callback) const {
                std::unique_lock<std::shared_mutex> lock(this->lock_);

                if (this->data_.empty()) {
                    return false;
                }

                callback(this->data_.front());

                return true;
            };

            ValueType copy_value(int index) const {
                ValueType copy = [&] {
                    std::shared_lock<std::shared_mutex> lock(this->lock_);
                    return this->data_.at(index);
                }();
                return copy;
            }

            template<typename Transformer, class... Args>
            bool on_front(Transformer callback, Args&&... args) const {
                std::unique_lock<std::shared_mutex> lock(this->lock_);

                if (this->data_.empty()) {
                    return false;
                }

                callback(this->data_.front(), std::forward<Args>(args)...);

                return true;
            };

            template<typename Transformer>
            auto on_back(Transformer callback) const {
                ValueType copy = [&] {
                    std::shared_lock<std::shared_mutex> lock(this->lock_);
                    return this->data_.back();
                }();
                return callback(copy);
            }

            template<typename Transformer, class... Args>
            auto on_back(Transformer callback, Args&&... args) const {
                ValueType copy = [&] {
                    std::shared_lock<std::shared_mutex> lock(this->lock_);
                    return this->data_.back();
                }();
                return callback(copy, std::forward<Args>(args)...);
            }

            template<typename Predicate>
            size_t count_if(Predicate predicate) const {
                std::shared_lock<std::shared_mutex> lock(this->lock_);
                return std::count_if(this->data_.begin(), this->data_.end(), predicate);
            }

            size_t count(const ValueType& value) const {
                std::shared_lock<std::shared_mutex> lock(this->lock_);
                return std::count(this->data_.begin(), this->data_.end(), value);
            }

            void assign(size_t count, const ValueType& value) {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                this->data_.assign(count, value);
            }

            template<class InputIterator>
            void assign(InputIterator first, InputIterator last) {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                this->data_.assign(first, last);
            }

            void assign(std::initializer_list<ValueType> ilist) {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                this->data_.assign(ilist);
            }

            void clear() noexcept {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                this->data_.clear();
            };

            void push_back(const ValueType& value) {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                this->data_.push_back(value);
            }

            void push_back(ValueType&& value) {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                this->data_.push_back(std::move(value));
            }

            template<class... Args>
            void emplace_back(Args&&... args) {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                this->data_.emplace_back(std::forward<Args>(args)...);
            }

            void pop_back() {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                if (!this->data_.empty()) {
                    this->data_.pop_back();
                }
            }

            std::optional<ValueType> remove_at(std::size_t index){
                std::lock_guard<std::shared_mutex> lock(this->lock_);

                if (index >= this->data_.size()) {
                    return std::nullopt;
                }

                ValueType removed_value = std::move(this->data_[index]);
                this->data_.erase(this->data_.begin() + static_cast<std::ptrdiff_t>(index));

                return removed_value;
            }

            template<typename Predicate>
            std::size_t erase_if(Predicate predicate) {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                std::size_t initial_size = this->data_.size();
                this->data_.erase(
                    std::remove_if(this->data_.begin(), this->data_.end(), predicate),
                    this->data_.end()
                );
                return initial_size - this->data_.size();
            }

            size_t size() const {
                std::shared_lock lock(this->lock_);
                return this->data_.size();
            }

            bool empty() const {
                std::shared_lock lock(this->lock_);
                return this->data_.empty();
            }

            size_t max_size() const {
                std::shared_lock lock(this->lock_);
                return this->data_.max_size();
            }

            void reserve(size_t new_capacity) {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                this->data_.reserve(new_capacity);
            }

            size_t capacity() const {
                std::shared_lock lock(this->lock_);
                return this->data_.capacity();
            }

            void resize(size_t new_size) {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                this->data_.resize(new_size);
            }

            void resize(size_t new_size, const ValueType& value) {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                this->data_.resize(new_size, value);
            }

            template<typename Consumer>
            void for_each(Consumer callback) const {
                std::shared_lock lock(this->lock_);
                for (const ValueType& value : this->data_) {
                    callback(value);
                }
            }

            template<typename Mapper>
            auto map(Mapper callback) const {
                std::vector<ValueType> snapshot;
                {
                    std::shared_lock lock(this->lock_);
                    snapshot = this->data_;
                }
                using ReturnType = std::decay_t<
                    decltype(callback(std::declval<const ValueType&>()))
                >;

                std::vector<ReturnType> result;
                result.reserve(snapshot.size());
                for (ValueType& value : snapshot) {
                    result.push_back(callback(value));
                }
                return result;
            }
        private:
            std::vector<ValueType> data_;
            mutable std::shared_mutex lock_;
    };

    inline std::string replace_all_text(
        const std::string_view& template_text,
        const std::string_view placeholder,
        const std::string_view replacement
    ) {
        std::string result{template_text};
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), replacement);
            pos += replacement.length();
        }
        return result;
    }

    [[noreturn]] inline void throw_error(
        const std::string& error_type,
        const std::string& error_message,
        const std::string& file,
        const int line,
        const std::string& function,
        const std::string& message_format = "%(message)s"
    ) {
        std::string formatted_message = replace_all_text(
            message_format,
            "%(message)",
            error_message
        );
        formatted_message = replace_all_text(
            formatted_message,
            "%(lineno)d",
            std::to_string(line)
        );

        formatted_message = replace_all_text(
            formatted_message,
            "%(filename)",
            file
        );

        formatted_message = replace_all_text(
            formatted_message,
            "%(funcName)s",
            function
        );

        formatted_message = replace_all_text(
            formatted_message,
            "%(exc_info)s",
            error_type
        );

        if (error_type == "logic_error") {
            throw std::logic_error(formatted_message);
        }

        if (error_type == "invalid_argument") {
            throw std::invalid_argument(formatted_message);
        }

        if (error_type == "out_of_range") {
            throw std::out_of_range(formatted_message);
        }
        if (error_type == "domain_error") {
            throw std::domain_error(formatted_message);
        }
        if (error_type == "length_error") {
            throw std::length_error(formatted_message);
        }

        if (error_type == "out_of_range") {
            throw std::out_of_range(formatted_message);
        }

        if (error_type == "overflow_error") {
            throw std::overflow_error(formatted_message);
        }

        if (error_type == "underflow_error") {
            throw std::underflow_error(formatted_message);
        }

        throw std::runtime_error(formatted_message);
    }

}
#define DEFAULT_ERROR_FORMAT "%(filename)s%(funcName)s#%(lineno)d %(exc_info)s: %(message)s"
#define RUNTIME_ERROR(message, fmt) owplog::utility::throw_error("runtime_error", message, __FILE__, __LINE__, __func__, fmt)
#define THROW_RUNTIME_ERROR(message) owplog::utility::throw_error("runtime_error", message, __FILE__, __LINE__, __func__, DEFAULT_ERROR_FORMAT)
#define LOGIC_ERROR(message, fmt) owplog::utility::throw_error("logic_error", message, __FILE__, __LINE__, __func__, fmt)
#define THROW_LOGIC_ERROR(message) owplog::utility::throw_error("logic_error", message, __FILE__, __LINE__, __func__, DEFAULT_ERROR_FORMAT)
#define INVALID_ARGUMENT(message, fmt) owplog::utility::throw_error("invalid_argument", message, __FILE__, __LINE__, __func__, fmt)
#define THROW_INVALID_ARGUMENT(message) owplog::utility::throw_error("invalid_argument", message, __FILE__, __LINE__, __func__, DEFAULT_ERROR_FORMAT)
#define OUT_OF_RANGE(message, fmt) owplog::utility::throw_error("out_of_range", message, __FILE__, __LINE__, __func__, fmt)
#define THROW_OUT_OF_RANGE(message) owplog::utility::throw_error("out_of_range", message, __FILE__, __LINE__, __func__, DEFAULT_ERROR_FORMAT)
#define DOMAIN_ERROR(message, fmt) owplog::utility::throw_error("domain_error", message, __FILE__, __LINE__, __func__, fmt)
#define THROW_DOMAIN_ERROR(message) owplog::utility::throw_error("domain_error", message, __FILE__, __LINE__, __func__, DEFAULT_ERROR_FORMAT)
#define LENGTH_ERROR(message, fmt) owplog::utility::throw_error("length_error", message, __FILE__, __LINE__, __func__, fmt)
#define THROW_LENGTH_ERROR(message) owplog::utility::throw_error("length_error", message, __FILE__, __LINE__, __func__, DEFAULT_ERROR_FORMAT)
#define OVERFLOW_ERROR(message, fmt) owplog::utility::throw_error("overflow_error", message, __FILE__, __LINE__, __func__, fmt)
#define THROW_OVERFLOW_ERROR(message) owplog::utility::throw_error("overflow_error", message, __FILE__, __LINE__, __func__, DEFAULT_ERROR_FORMAT)
#define UNDERFLOW_ERROR(message, fmt) owplog::utility::throw_error("underflow_error", message, __FILE__, __LINE__, __func__, fmt)
#define THROW_UNDERFLOW_ERROR(message) owplog::utility::throw_error("underflow_error", message, __FILE__, __LINE__, __func__, DEFAULT_ERROR_FORMAT)
#endif //OWPLOG_UTILITY_H
