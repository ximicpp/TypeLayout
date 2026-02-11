// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_FIXED_STRING_HPP
#define BOOST_TYPELAYOUT_FIXED_STRING_HPP

#include <boost/typelayout/fwd.hpp>
#include <ostream>

namespace boost {
namespace typelayout {

    // =========================================================================
    // FixedString<N> -- Compile-Time Fixed-Size String
    // =========================================================================

    template <size_t N>
    struct FixedString {
        char value[N];
        static constexpr size_t size = N - 1;

        constexpr FixedString() : value{} {}

        constexpr FixedString(const char (&str)[N]) {
            for (size_t i = 0; i < N; ++i)
                value[i] = str[i];
        }

        constexpr FixedString(std::string_view sv) : value{} {
            for (size_t i = 0; i < N - 1 && i < sv.size(); ++i)
                value[i] = sv[i];
        }

        template <size_t M>
        constexpr auto operator+(const FixedString<M>& other) const noexcept {
            constexpr size_t new_size = N + M - 1;
            char result[new_size] = {};
            size_t pos = 0;

            while (pos < N - 1 && value[pos] != '\0') {
                result[pos] = value[pos];
                ++pos;
            }

            size_t j = 0;
            while (j < M) {
                result[pos++] = other.value[j++];
            }

            return FixedString<new_size>(result);
        }

        template <size_t M>
        constexpr bool operator==(const FixedString<M>& other) const noexcept {
            size_t i = 0;
            while (i < N && i < M) {
                if (value[i] != other.value[i]) return false;
                if (value[i] == '\0') return true;
                ++i;
            }
            if (i < N) return value[i] == '\0';
            if (i < M) return other.value[i] == '\0';
            return true;
        }

        constexpr bool operator==(const char* other) const noexcept {
            for (size_t i = 0; i < N; ++i) {
                if (value[i] != other[i]) return false;
                if (value[i] == '\0') return true;
            }
            // FixedString always contains '\0' within N bytes,
            // so this point is unreachable in practice.
            return true;
        }

        constexpr size_t length() const noexcept {
            size_t len = 0;
            while (len < N && value[len] != '\0') ++len;
            return len;
        }

        constexpr operator std::string_view() const noexcept {
            return {value, length()};
        }

        // Strip leading character (used to remove leading comma after fold-expression).
        consteval auto skip_first() const noexcept {
            char result[N] = {};
            if (value[0] != '\0') {
                for (size_t i = 1; i < N; ++i)
                    result[i - 1] = value[i];
            }
            return FixedString<N>(result);
        }
    };

    template<size_t N>
    std::ostream& operator<<(std::ostream& os, const FixedString<N>& str) {
        return os << str.value;
    }

    // Free function: integer to FixedString<21> (uint64_t max = 20 digits + '\0').
    // Writes digits right-to-left, no reversal needed.
    template <typename T>
    constexpr FixedString<21> to_fixed_string(T num) noexcept {
        constexpr int last = 19; // rightmost digit position (buf[20] = '\0')

        if (num == 0) {
            char result[21] = {};
            result[0] = '0';
            return FixedString<21>(result);
        }

        bool negative = std::is_signed_v<T> && num < 0;
        using UnsignedT = std::make_unsigned_t<T>;
        UnsignedT abs_num = negative
            ? UnsignedT(-(std::make_signed_t<T>(num)))
            : UnsignedT(num);

        char buf[21] = {};
        int pos = last;
        while (abs_num > 0) {
            buf[pos--] = '0' + char(abs_num % 10);
            abs_num /= 10;
        }
        if (negative)
            buf[pos--] = '-';

        // Shift to front
        int start = pos + 1;
        int len = last - start + 1;
        char result[21] = {};
        for (int i = 0; i < len; ++i)
            result[i] = buf[start + i];
        return FixedString<21>(result);
    }

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_FIXED_STRING_HPP
