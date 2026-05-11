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

namespace owplog::utility {
    template<typename ValueType>
    struct IsProtectedNumeric : std::integral_constant<
        bool,
        std::is_arithmetic_v<ValueType> &&
        !std::is_same_v<ValueType, bool> &&
        !std::is_same_v<ValueType, char> &&
        !std::is_same_v<ValueType, signed char> &&
        !std::is_same_v<ValueType, unsigned char>
    > {};

    template<typename ValueType>
    class ProtectedBase {
        public:
            explicit ProtectedBase(ValueType value) : value_(std::move(value)) {}
            virtual ~ProtectedBase() = default;
            ProtectedBase(const ProtectedBase& other) {
                ValueType copy = other.read_copy();
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                this->value_ = copy;
            };
            ProtectedBase& operator=(const ProtectedBase& other) {
                ValueType copy = other.read_copy();
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                this->value_ = copy;
                return *this;
            };
            ProtectedBase& operator=(const ValueType& other) {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                this->value_ = other;
                return *this;
            }
            ProtectedBase& operator=(ValueType&& other) {
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

    template<typename ValueType, bool IsNumericValue = IsProtectedNumeric<ValueType>::value>
    class Protected;

    template<typename ValueType>
    class Protected<ValueType, false> : public ProtectedBase<ValueType> {
        public:
            using ProtectedBase<ValueType>::ProtectedBase;
            using ProtectedBase<ValueType>::operator=;
    };

    template<typename ValueType>
    class Protected<ValueType, true> : public ProtectedBase<ValueType> {
        public:
            using ProtectedBase<ValueType>::ProtectedBase;
            using ProtectedBase<ValueType>::operator=;

            ValueType increment(ValueType amount = ValueType{1}) {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                this->value_ += amount;
                return this->value_;
            }

            ValueType decrement(ValueType amount = ValueType{1}) {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                this->value_ -= amount;
                return this->value_;
            };

            ValueType operator/(ValueType other) const {
                std::shared_lock lock(this->lock_);
                return this->value_ / other;
            }

            ValueType operator+(ValueType other) const {
                std::shared_lock lock(this->lock_);
                return this->value_ + other;
            };

            ValueType operator-(ValueType other) const {
                std::shared_lock lock(this->lock_);
                return this->value_ - other;
            }

            ValueType operator*(ValueType other) const {
                std::shared_lock lock(this->lock_);
                return this->value_ * other;
            }

            ValueType operator++() {
                return this->increment(1);
            }
            ValueType operator++(int) {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                ValueType old_value = this->value_;
                this->value_ += 1;
                return old_value;
            }
            ValueType operator--() {
                return this->decrement(1);
            }
            ValueType operator--(int) {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                ValueType old_value = this->value_;
                this->value_ -= 1;
                return old_value;
            }

            bool operator==(const ValueType& other) const {
                std::shared_lock lock(this->lock_);
                return this->value_ == other;
            }
            bool operator!=(const ValueType& other) const {
                std::shared_lock lock(this->lock_);
                return this->value_ != other;
            }
            bool operator>=(const ValueType& other) const {
                std::shared_lock lock(this->lock_);
                return this->value_ >= other;
            }
            bool operator<=(const ValueType& other) const {
                std::shared_lock lock(this->lock_);
                return this->value_ <= other;
            }
            bool operator>(const ValueType& other) const {
                std::shared_lock lock(this->lock_);
                return this->value_ > other;
            }
            bool operator<(const ValueType& other) const {
                std::shared_lock lock(this->lock_);
                return this->value_ < other;
            }
    };

    template<typename ValueType>
    class ProtectedVector {
        public:
            ProtectedVector() = default;
            ~ProtectedVector() = default;
            explicit ProtectedVector(std::initializer_list<ValueType> initial_data) : data_(initial_data) {}
            explicit ProtectedVector(std::vector<ValueType> initial_data) : data_(std::move(initial_data)) {}

            template<typename Transformer>
            auto on(size_t index, Transformer callback) const {
                std::shared_lock<std::shared_mutex> lock(this->lock_);
                return callback(this->data_.at(index));
            };

            template<typename Transformer>
            auto on_front(Transformer callback) const {
                std::shared_lock<std::shared_mutex> lock(this->lock_);
                return callback(this->data_.front());
            };

            template<typename Transformer>
            auto on_back(Transformer callback) const {
                std::lock_guard<std::shared_mutex> lock(this->lock_);
                return callback(this->data_.back());
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
                using ReturnType = std::decay_t<
                    decltype(callback(std::declval<const ValueType&>()))
                >;
                std::shared_lock lock(this->lock_);
                std::vector<ReturnType> result;
                result.reserve(this->data_.size());
                for (auto& value : this->data_) {
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
