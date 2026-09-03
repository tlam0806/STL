#pragma once
#include <iostream>



template<typename T>
class CircularQueue {
public : 
    class iterator {
    public : 
        iterator(T* data, std::size_t index, std::size_t capacity) :
            data_{data},
            index_{index},
            capacity_{capacity}
        {}

        T& operator*() const {
            return data_[index_ % capacity_];
        }

        iterator& operator++() {
            ++index_;
            return *this;
        }
        
        bool operator !=(const iterator& other) const {
            return index_ != other.index_;
        }

    private : 
        T* data_;
        std::size_t index_;
        std::size_t capacity_;

    };
    CircularQueue() :
        capacity_{1},
        size_{0},
        head_{0}, // last occupied
        data_{allocate_raw(1)}
    {
        
    }

    iterator begin() const {
        // std::cout << head_ << "begin\n";
        return iterator(data_, head_, capacity_);
    }

    iterator end() const {
        // std::cout << size_ << "end\n";
        return iterator(data_, (head_ + size_), capacity_);
    }

    bool empty() const {
        return size_ == 0;
    }

    std::size_t size() const {
        return size_;
    }

    T& operator[](std::size_t index) {
        return data_[(head_ + index) % capacity_];
    }

    void push_front(const T& element) {
        emplace_front(element);
    }

    void push_front(T&& element) {
        emplace_front(std::move(element));
    }

    void push_back(const T& element) {
        emplace_back(element);
    }

    void push_back(T&& element) {
        emplace_back(std::move(element));
    }

    template<typename... Args>
    T& emplace_back(Args&&... args) {
        if (size_ == capacity_) {
            T tmp = T(std::forward<Args>(args)...);
            reserve(capacity_ * 2);
            return *std::construct_at(
                data_ + (head_ + size_++) % capacity_, 
                std::move_if_noexcept(tmp)
            );
        } else {
            return *std::construct_at(
                data_ + (head_ + size_++) % capacity_,
                T(std::forward<Args>(args)...)
            );
        }
    }

    template<typename... Args>
    T& emplace_front(Args&&... args) {
        if (size_ == capacity_) {
            T tmp = T(std::forward<Args>(args)...);
            reserve(capacity_ * 2);
            head_ = (head_ - 1 + capacity_) % capacity_;
            ++size_;
            return *std::construct_at(
                data_ + head_, 
                std::move_if_noexcept(tmp)
            );
        } else {
            head_ = (head_ - 1 + capacity_) % capacity_;
            ++size_;
            return *std::construct_at(
                data_ + head_,
                std::forward<Args>(args)...
            );
        }
    }

    T& back() {
        std::cout << head_ << " " << size_ << " " << capacity_ << "\n";
        return data_[(head_ + size_ - 1 + capacity_) % capacity_];
    }
    
    T& front() {
        return data_[head_];
    }

    void reserve(std::size_t new_capacity) {
        T* new_data = allocate_raw(new_capacity);
        for (std::size_t i = 0; i < size_; ++i) {
            T* old_addr = data_ + (head_ + i) % capacity_;
            std::construct_at(
                new_data + i,
                std::move_if_noexcept(*old_addr)
            );
            std::destroy_at(
                old_addr
            );
        }
        ::operator delete(data_);
        data_ = new_data;
        capacity_ = new_capacity;
        head_ = 0;
    }

private : 
    std::size_t capacity_ = 0;
    std::size_t size_;
    std::size_t head_;
    T* data_;

    static T* allocate_raw (std::size_t count) {
        if (count == 0) {
            return nullptr;
        }
        return reinterpret_cast<T*>(::operator new(count * sizeof(T)));
    }

    
};
