// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_FIXED_STRING_HPP
#define BOOST_TYPELAYOUT_FIXED_STRING_HPP

#include <boost/typelayout/detail/sig_parser.hpp>
#include <boost/typelayout/fwd.hpp>
#include <compare>
#include <ostream>

namespace boost {
namespace typelayout {
inline namespace v1 {

    // FixedString<N> -- compile-time fixed-size string (N = char count, excl. null).
    // TODO(P2484): Replace with std::basic_fixed_string when standardized.

    template <size_t N>
    struct FixedString {
        char value[N + 1];
        static constexpr size_t size = N;

        constexpr FixedString() : value{} {}

        constexpr FixedString(const char (&str)[N + 1]) {
            for (size_t i = 0; i <= N; ++i)
                value[i] = str[i];
        }

        constexpr FixedString(std::string_view sv) : value{} {
            for (size_t i = 0; i < N && i < sv.size(); ++i)
                value[i] = sv[i];
        }

        template <size_t M>
        constexpr auto operator+(const FixedString<M>& other) const noexcept {
            constexpr size_t new_size = N + M;
            char result[new_size + 1] = {};
            for (size_t i = 0; i < N; ++i)
                result[i] = value[i];
            for (size_t i = 0; i <= M; ++i)
                result[N + i] = other.value[i];
            return FixedString<new_size>(result);
        }

        template <size_t M>
        constexpr bool operator==(const FixedString<M>& other) const noexcept {
            size_t i = 0;
            while (i <= N && i <= M) {
                if (value[i] != other.value[i]) return false;
                if (value[i] == '\0') return true;
                ++i;
            }
            if (i <= N) return value[i] == '\0';
            if (i <= M) return other.value[i] == '\0';
            return true;
        }

        constexpr bool operator==(const char* other) const noexcept {
            for (size_t i = 0; i <= N; ++i) {
                if (value[i] != other[i]) return false;
                if (value[i] == '\0') return true;
            }
            return true;
        }

        template <size_t M>
        constexpr std::strong_ordering operator<=>(const FixedString<M>& other) const noexcept {
            size_t i = 0;
            while (i < N && i < M && value[i] != '\0' && other.value[i] != '\0') {
                if (value[i] < other.value[i]) return std::strong_ordering::less;
                if (value[i] > other.value[i]) return std::strong_ordering::greater;
                ++i;
            }
            // Compare lengths: shorter string is "less" if prefix matches.
            size_t len_a = length();
            size_t len_b = other.length();
            return len_a <=> len_b;
        }

        constexpr size_t length() const noexcept {
            size_t len = 0;
            while (len < N && value[len] != '\0') ++len;
            return len;
        }

        constexpr const char* c_str() const noexcept { return value; }

        constexpr operator std::string_view() const noexcept {
            return {value, length()};
        }

        // Returns index of first occurrence of `needle`, or npos.
        template <size_t M>
        constexpr size_t find(const FixedString<M>& needle) const noexcept {
            size_t haystack_len = length();
            size_t needle_len = needle.length();
            if (needle_len == 0) return 0;
            if (needle_len > haystack_len) return npos;
            for (size_t i = 0; i <= haystack_len - needle_len; ++i) {
                bool match = true;
                for (size_t j = 0; j < needle_len; ++j) {
                    if (value[i + j] != needle.value[j]) {
                        match = false;
                        break;
                    }
                }
                if (match) return i;
            }
            return npos;
        }

        template <size_t M>
        constexpr bool contains(const FixedString<M>& needle) const noexcept {
            return find(needle) != npos;
        }

        // Token-boundary-aware contains: matches only when the preceding
        // character is not alphanumeric (prevents "ptr[" matching "nullptr[").
        template <size_t M>
        constexpr bool contains_token(const FixedString<M>& needle) const noexcept {
            size_t haystack_len = length();
            size_t needle_len = needle.length();
            if (needle_len == 0) return true;
            if (needle_len > haystack_len) return false;
            for (size_t i = 0; i <= haystack_len - needle_len; ++i) {
                bool match = true;
                for (size_t j = 0; j < needle_len; ++j) {
                    if (value[i + j] != needle.value[j]) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    if (i == 0) return true;
                    char prev = value[i - 1];
                    bool prev_is_alnum = (prev >= 'a' && prev <= 'z') ||
                                         (prev >= 'A' && prev <= 'Z') ||
                                         (prev >= '0' && prev <= '9');
                    if (!prev_is_alnum) return true;
                }
            }
            return false;
        }

        static constexpr size_t npos = static_cast<size_t>(-1);

        // Strip leading character; returns FixedString<N-1>.
        consteval auto skip_first() const noexcept {
            if constexpr (N == 0) {
                return FixedString<0>{};
            } else {
                char result[N] = {};
                for (size_t i = 1; i < N; ++i)
                    result[i - 1] = value[i];
                return FixedString<N - 1>(result);
            }
        }
    };

    // CTAD deduction guide: "hello" (type const char[6]) -> FixedString<5>
    template <size_t N>
    FixedString(const char (&)[N]) -> FixedString<N - 1>;

    template<size_t N>
    std::ostream& operator<<(std::ostream& os, const FixedString<N>& str) {
        return os << str.value;
    }

    // to_fixed_string<V>() -- exact-sized NTTP form.
    consteval size_t count_digits(size_t v) noexcept {
        if (v == 0) return 1;
        size_t n = 0;
        while (v > 0) { v /= 10; ++n; }
        return n;
    }

    template <size_t V>
    consteval auto to_fixed_string() noexcept {
        constexpr size_t digits = count_digits(V);
        char result[digits + 1] = {};
        if constexpr (V == 0) {
            result[0] = '0';
        } else {
            size_t pos = digits;
            size_t v = V;
            while (v > 0) { result[--pos] = '0' + char(v % 10); v /= 10; }
        }
        return FixedString<digits>(result);
    }

    // to_fixed_string(num) -- runtime form, returns FixedString<20>.
    template <typename T>
    constexpr FixedString<20> to_fixed_string(T num) noexcept {
        constexpr int last = 19; // rightmost digit position (buf[20] = '\0')

        if (num == 0) {
            char result[21] = {};
            result[0] = '0';
            return FixedString<20>(result);
        }

        bool negative = std::is_signed_v<T> && num < 0;
        using UnsignedT = std::make_unsigned_t<T>;
        UnsignedT abs_num = negative
            ? UnsignedT(0) - static_cast<UnsignedT>(num)
            : static_cast<UnsignedT>(num);

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
        return FixedString<20>(result);
    }

namespace detail {

template <typename Sig>
consteval bool sig_has_pointer(const Sig& sig) noexcept {
    return sig_has_pointer(std::string_view(sig));
}

} // namespace detail

} // inline namespace v1
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_FIXED_STRING_HPP
