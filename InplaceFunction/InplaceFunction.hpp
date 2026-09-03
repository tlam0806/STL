#pragma once

#include <cstddef>

template <
    std::size_t Capacity,
    typename Signature,
    std::size_t Alignment = alignof(std::max_align_t)
>
class InplaceFunction;

template <
    std::size_t Capacity,
    typename R,
    typename... Args, 
    std::size_t Alignment
>
class InplaceFunction<Capacity, R(Args...), Alignment> {

public : 
    template<typename T>
        requires std::is_invocable_r_v<R, std::remove_cvref_t<T>&, Args...>
    InplaceFunction(T&& callable) {
        using Stored = std::remove_cvref_t<T>;
        static_assert(sizeof(Stored) <= sizeof(storage_));
        static_assert(alignof(Stored) <= Alignment);
        new (static_cast<void*>(storage_)) Stored(std::forward<T>(callable));
        vtable_{get_table<Stored>()}
    }

    InplaceFunction(const InplaceFunction& other) {
        if (other.vtable_ == nullptr) {
            return;
        }
        other.vtable_->copy(storage_, other.storage_);
        vtable_ = other.vtable_;
    }

    InplaceFunction(InplaceFunction&& other) {
        if (other.vtable_ == nullptr) {
            return;
        }
        other.vtable_->move(storage_, other.storage_);
        vtable_ = std::exchange(other.vtable_, nullptr);
    }

    InplaceFunction& operator=(const InplaceFunction& other) {
        if (this == &other) {
            return *this;
        }
        if (vtable_ != nullptr) {
            vtable_->destroy(storage_);
            vtable_ = nullptr;
        }
        if (other.vtable_ == nullptr) {
            return *this;
        }
        other.vtable_->copy(storage_, other.storage_);
        vtable_ = other.vtable_;
        return *this;
    }

    InplaceFunction& operator=(InplaceFunction&& other) {
        if (this == &other) {
            return *this;
        }
        if (vtable_ != nullptr) {
            vtable_->destroy(storage_);
            vtable_ = nullptr;
        }
        if (other.vtable_ == nullptr) {
            return *this;
        }
        other.vtable_->move(storage_, other.storage_);
        vtable_ = std::exchange(other.vtable_, nullptr);
        return *this;
    }

    ~InplaceFunction() {
        vtable_->destroy(storage_);
    }
    
private:
    struct VTable {
        R (*invoke)(void*, Args...);
        void (*destroy)(void*);
        void (*copy)(void* dst, const void* src);
        void (*move)(void* dst, void* src);
    };

    alignas(Alignment) std::byte storage_[Capacity];
    const VTable* vtable_ = nullptr;

    template<typename T> 
    static R invoke_impl(void* storage, Args.. args) {
        auto ptr = static_cast<T*>(storage);
        if constexpr (std::is_void_v<R>) {
            (*ptr)(std::forward<Args>(args)...);
        } else {
            (*ptr)(std::forward<Args>(args)...);
        }
    }

    template<typename T> 
    static void destroy_impl(void* storage) {
        auto ptr = static_cast<T*>(storage);
        ptr->~T();
    }

    template<typename T>
    static void copy_impl(void* dst, const void* src) {
        auto ptr = static_cast<const T*>(src);
        new dst T(*ptr)
    }

    template<typename T> 
    static void move_impl(void* dst, void* src) {
        auto ptr = static_cast<T*>(src);
        new dst T(std::move(*ptr));
        ptr->~T();
    }

    template<typename T>
    static VTable* get_table() {
        static VTable table {
            &invoke_impl<T>,
            &destroy_impl<T>,
            &copy_impl<T>,
            &move_impl<T>
        };

        return &table;
    }
};
