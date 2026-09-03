#pragma once
#include <algorithm>
#include <type_traits>


template<typename T, typename... Args> 
struct max_size {
    static constexpr std::size_t value = std::max(sizeof(T), max_size<Args...>::value);
};

template<typename T>
struct max_size<T> {
    static constexpr std::size_t value = sizeof(T);
};

template<typename T, typename... Args>
struct max_align {
    static constexpr std::size_t value = std::max(alignof(T), max_align<Args...>::value);
};

template<typename T>
struct max_align<T> {
    static constexpr std::size_t value = alignof(T);
};

template<typename std::size_t Index, typename First, typename... Args>
struct type_at {
    using type = type_at<Index - 1, Args...>::type;
};

template<typename First, typename... Args> 
struct type_at<0, First, Args...> {
    using type = First;
};

template<typename>
inline constexpr bool always_false = false;

template<std::size_t I, typename T, typename... Args>
struct type_matching;

template<std::size_t I, typename T, typename First, typename... Args> 
struct type_matching<I, T, First, Args...> {
    using type = type_matching<I + 1, T, Args...>::type;
    static constexpr std::size_t index = type_matching<I + 1, T, Args...>::index;
};

template<std::size_t I, typename T, typename... Args> 
struct type_matching<I, T, T, Args...> {
    using type = T;
    static constexpr std::size_t index = I;
};  

template<std::size_t I, typename T>
struct type_matching<I, T> {
    static_assert(
        always_false<T>,
        "type matching : requested type does not exits"
    );
};

template <typename... Args>
class variant {
    static_assert(
        (std::is_same_v<Args, std::remove_cvref_t<Args>> && ...),
        "variant alternatives must be non-cv value type"
    );
public :    
    variant() {
        using type = type_at<0, Args...>::type;
        index_ = 0;
        std::construct_at(
            reinterpret_cast<type*> (storage_)
        );
    }  

    template<typename T> 
    variant(T&& object) {
        using type = std::remove_cvref_t<T>;
        index_ = type_matching<0, type, Args...>::index;
        std::construct_at(
            reinterpret_cast<type*> (storage_),
            std::forward<T>(object)
            // object
        );
    }

    ~variant() {
        destroy<0, Args...>();
    }

    template<std::size_t I>
    decltype(auto) get() {
        static_assert(
            I < sizeof...(Args) && I >= 0,
            "variant::get: index out of range"
        );
        using type = type_at<I, Args...>::type;
        return *reinterpret_cast<type*>(storage_);
    }

    template<typename T> 
    decltype(auto) get() {
        static constexpr std::size_t expected_index = type_matching<0, T, Args...>::index;
        if (index_ != expected_index) {
            throw std::bad_variant_access{};
        }
        return *reinterpret_cast<T*>(storage_);
    }

    template<std::size_t I>
    decltype(auto) get() const {
        static_assert(
            I < sizeof...(Args) && I >= 0,
            "variant::get: index out of range"
        );
        using type = type_at<I, Args...>::type;
        return *reinterpret_cast<const type*>(storage_);
    }

    template<typename T> 
    decltype(auto) get() const {
        static constexpr std::size_t expected_index = type_matching<0, T, Args...>::index;
        if (index_ != expected_index) {
            throw std::bad_variant_access{};
        }
        return *reinterpret_cast<const T*>(storage_);
    }

private : 
    alignas(max_align<Args...>::value)
    std::byte storage_[max_size<Args...>::value];
    std::size_t index_;

    template<std::size_t I, typename T, typename... Rest> 
    void destroy() {
        if (I == index_) {
            std::destroy_at(
                reinterpret_cast<T*>(storage_)
            );
            return;
        }
        if constexpr (sizeof...(Rest)) {
            destroy<I + 1, Rest...>();
        }
    }
};
