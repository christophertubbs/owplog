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
#include <utility>
#include <mutex>
#include <cstddef>
#include <deque>
#include <functional>

namespace owplog::utility {
    template<typename ValueType>
    class Protected {
        public:
            Protected() = default;
            explicit Protected(ValueType value) : value_(std::move(value)) {}
            ~Protected() = default;
            Protected(const Protected& other) : value_(other.read_copy()) {};
            Protected& operator=(const Protected& other) {
                if (this == &other) {
                    return *this;
                }
                ValueType copy = other.read_copy();
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                this->value_ = copy;
                return *this;
            };
            Protected& operator=(const ValueType& other) {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                this->value_ = other;
                return *this;
            }
            Protected& operator=(ValueType&& other) {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                this->value_ = std::move(other);
                return *this;
            }

            ValueType read_copy() const {
                std::shared_lock lock(this->lock_);
                return this->value_;
            }

            template<typename CallbackType>
            decltype(auto) read(CallbackType callback) const {
                std::shared_lock lock(this->lock_);
                return callback(this->value_);
            }

            void set(ValueType value) {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                this->value_ = std::move(value);
            }
        protected:
            mutable std::shared_mutex lock_;
            ValueType value_;
    };

    template<typename ValueType>
    class ProtectedDeque {
        public:
            ProtectedDeque() = default;
            ~ProtectedDeque() = default;
            explicit ProtectedDeque(std::initializer_list<ValueType> initial_data) : data_(initial_data.begin(), initial_data.end()) {}
            explicit ProtectedDeque(std::deque<ValueType> initial_data) : data_(std::move(initial_data)) {}

            [[nodiscard]] std::size_t size() const {
                std::shared_lock lock(this->lock_);
                return this->data_.size();
            };

            [[nodiscard]] std::size_t max_size() const {
                std::shared_lock lock(this->lock_);
                return this->data_.max_size();
            }

            [[nodiscard]] bool empty() const {
                std::shared_lock lock(this->lock_);
                return this->data_.empty();
            }

            void clear() {
                std::unique_lock lock(this->lock_);
                this->data_.clear();
            };

            void push_back(const ValueType& value) {
                std::unique_lock lock(this->lock_);
                this->data_.push_back(value);
            }

            void push_back(ValueType&& value) {
                std::unique_lock lock(this->lock_);
                this->data_.push_back(std::move(value));
            }

            void push_front(const ValueType& value) {
                std::unique_lock lock(this->lock_);
                this->data_.push_front(value);
            }

            void push_front(ValueType&& value) {
                std::unique_lock lock(this->lock_);
                this->data_.push_front(std::move(value));
            }

            std::optional<ValueType> pop_front() {
                std::unique_lock lock(this->lock_);
                if (this->data_.empty()) {
                    return std::nullopt;
                }
                ValueType front_value = std::move(this->data_.front());
                this->data_.pop_front();
                return front_value;
            }

            std::optional<ValueType> pop_back() {
                std::unique_lock lock(this->lock_);
                if (this->data_.empty()) {
                    return std::nullopt;
                }
                ValueType back_value = std::move(this->data_.back());
                this->data_.pop_back();
                return back_value;
            }

            void resize(std::size_t size) {
                std::unique_lock lock(this->lock_);
                this->data_.resize(size);
            }

            void resize(std::size_t size, const ValueType& fill_value) {
                std::unique_lock lock(this->lock_);
                this->data_.resize(size, fill_value);
            }

            template<class... Args>
            void emplace_back(Args&&... args) {
                std::unique_lock lock(this->lock_);
                this->data_.emplace_back(std::forward<Args>(args)...);
            }

            template<class... Args>
            void emplace_front(Args&&... args) {
                std::unique_lock lock(this->lock_);
                this->data_.emplace_front(std::forward<Args>(args)...);
            }

        private:
            std::deque<ValueType> data_;
            mutable std::shared_mutex lock_;
    };

    template<typename ValueType>
    class ProtectedVector {
        public:
            ProtectedVector() = default;
            ~ProtectedVector() = default;
            explicit ProtectedVector(std::initializer_list<ValueType> initial_data) : data_(initial_data) {}
            explicit ProtectedVector(std::vector<ValueType> initial_data) : data_(std::move(initial_data)) {}

            template<typename Operator, class... Args>
            auto on_range(Operator function, Args&&... args) const {
                std::vector<ValueType> snapshot;
                {
                    std::shared_lock<std::shared_mutex> lock(this->lock_);
                    snapshot = this->data_;
                }

                return function(snapshot.begin(), snapshot.end(), std::forward<Args>(args)...);
            }

            template<typename Predicate>
            bool all_of(Predicate predicate) const {
                std::shared_lock<std::shared_mutex> lock(this->lock_);
                return std::all_of(this->data_.begin(), this->data_.end(), predicate);
            }

            template<typename Predicate>
            bool any_of(Predicate predicate) const {
                std::shared_lock<std::shared_mutex> lock(this->lock_);
                return std::any_of(this->data_.begin(), this->data_.end(), predicate);
            }

            template<typename Predicate>
            bool none_of(Predicate predicate) const {
                std::shared_lock<std::shared_mutex> lock(this->lock_);
                return std::none_of(this->data_.begin(), this->data_.end(), predicate);
            }

            template<typename Transformer>
            auto on_index(size_t index, Transformer callback) const {
                ValueType copy;
                {
                    std::shared_lock<std::shared_mutex> lock(this->lock_);
                    copy = this->data_.at(index);
                }

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
                std::shared_lock<std::shared_mutex> lock(this->lock_);
                for (const auto& item : this->data_) {
                    if (selector(item, std::forward<Args>(args)...)) {
                        callback(item);
                    }
                }
            }

            template<typename Transformer>
            auto on_front(Transformer callback) const {
                ValueType copy = [&] {
                    std::shared_lock<std::shared_mutex> lock(this->lock_);
                    return this->data_.front();
                }();
                return callback(copy);
            };

            template<typename Transformer, class... Args>
            auto on_front(Transformer callback, Args&&... args) const {
                ValueType copy = [&] {
                    std::shared_lock<std::shared_mutex> lock(this->lock_);
                    return this->data_.front();
                }();
                return callback(copy, std::forward<Args>(args)...);
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
                this->data_.pop_back();
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
                for (auto& value : this->data_) {
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
                for (auto& value : snapshot) {
                    result.push_back(callback(value));
                }
                return result;
            }
        private:
            std::vector<ValueType> data_;
            mutable std::shared_mutex lock_;
    };
}
#endif //OWPLOG_UTILITY_H
