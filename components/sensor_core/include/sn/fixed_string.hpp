#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace sn {

// NUL-terminated string with inline storage for up to N characters, so
// settings can live in structs that are copied to NVS and C APIs as-is.
template <std::size_t N>
class FixedString {
public:
    static constexpr std::size_t kCapacity = N;

    // Returns false (and leaves the string unchanged) when `text` does not fit.
    constexpr bool assign(std::string_view text) {
        if (text.size() > N) {
            return false;
        }
        for (std::size_t i = 0; i < text.size(); ++i) {
            data_[i] = text[i];
        }
        data_[text.size()] = '\0';
        size_ = text.size();
        return true;
    }

    [[nodiscard]] constexpr std::string_view view() const { return {data_.data(), size_}; }
    [[nodiscard]] constexpr const char* c_str() const { return data_.data(); }
    [[nodiscard]] constexpr std::size_t size() const { return size_; }
    [[nodiscard]] constexpr bool empty() const { return size_ == 0; }

    friend constexpr bool operator==(const FixedString& a, std::string_view b) {
        return a.view() == b;
    }

private:
    std::array<char, N + 1> data_{};
    std::size_t size_ = 0;
};

}  // namespace sn
