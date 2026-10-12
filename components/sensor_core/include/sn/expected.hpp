#pragma once

#include <utility>
#include <variant>

namespace sn {

// Minimal stand-in for std::expected (C++23), kept tiny so the core library
// builds unchanged on the host compiler and the ESP-IDF toolchain.
template <typename E>
struct Unexpected {
    E error;
};

template <typename E>
constexpr Unexpected<E> unexpected(E e) {
    return {e};
}

template <typename T, typename E>
class Expected {
public:
    // Implicit on purpose, mirroring std::expected: `return value;` and
    // `return unexpected(err);` both convert.
    // cppcheck-suppress noExplicitConstructor
    constexpr Expected(T value) : storage_(std::in_place_index<0>, std::move(value)) {}
    // cppcheck-suppress noExplicitConstructor
    constexpr Expected(Unexpected<E> u) : storage_(std::in_place_index<1>, u.error) {}

    [[nodiscard]] constexpr bool has_value() const { return storage_.index() == 0; }
    constexpr explicit operator bool() const { return has_value(); }

    [[nodiscard]] constexpr const T& value() const { return std::get<0>(storage_); }
    [[nodiscard]] constexpr const T& operator*() const { return value(); }
    [[nodiscard]] constexpr const T* operator->() const { return &value(); }
    [[nodiscard]] constexpr const E& error() const { return std::get<1>(storage_); }

private:
    std::variant<T, E> storage_;
};

}  // namespace sn
