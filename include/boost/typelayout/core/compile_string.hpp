// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_CORE_COMPILE_STRING_HPP
#define BOOST_TYPELAYOUT_CORE_COMPILE_STRING_HPP

#include <boost/typelayout/core/config.hpp>
#include <string_view>
#include <ostream>

namespace boost {
namespace typelayout {

    template <size_t N>
    struct CompileString {
        char value[N];
        static constexpr size_t size = N - 1;

        constexpr CompileString() : value{} {}

        constexpr CompileString(const char (&str)[N]) {
            for (size_t i = 0; i < N; ++i)
                value[i] = str[i];
        }

        constexpr CompileString(std::string_view sv) : value{} {
            for (size_t i = 0; i < N - 1 && i < sv.size(); ++i)
                value[i] = sv[i];
        }

        template <typename T>
        static constexpr CompileString<32> from_number(T num) noexcept {
            char result[32] = {};
            int idx = 0;
            
            if (num == 0) {
                result[0] = '0';
                idx = 1;
            } else {
                bool negative = std::is_signed_v<T> && num < 0;
                using UnsignedT = std::make_unsigned_t<T>;
                UnsignedT abs_num;

                if (negative)
                    abs_num = UnsignedT(-(std::make_signed_t<T>(num)));
                else
                    abs_num = UnsignedT(num);

                while (abs_num > 0) {
                    result[idx++] = '0' + char(abs_num % 10);
                    abs_num /= 10;
                }

                if (negative)
                    result[idx++] = '-';

                for (int i = 0; i < idx / 2; ++i) {
                    char temp = result[i];
                    result[i] = result[idx - 1 - i];
                    result[idx - 1 - i] = temp;
                }
            }
            result[idx] = '\0';
            return CompileString<32>(result);
        }

        template <size_t M>
        constexpr auto operator+(const CompileString<M>& other) const noexcept {
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
            
            return CompileString<new_size>(result);
        }

        template <size_t M>
        constexpr bool operator==(const CompileString<M>& other) const noexcept {
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
            return other[N] == '\0';
        }

        constexpr size_t length() const noexcept {
            size_t len = 0;
            while (len < N && value[len] != '\0') ++len;
            return len;
        }

        // Strip leading character (used to remove leading comma after fold-expression).
        consteval auto skip_first() const noexcept {
            char result[N] = {};
            if (value[0] != '\0') {
                for (size_t i = 1; i < N; ++i)
                    result[i - 1] = value[i];
            }
            return CompileString<N>(result);
        }
    };

    template<size_t N>
    std::ostream& operator<<(std::ostream& os, const CompileString<N>& str) {
        return os << str.value;
    }

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CORE_COMPILE_STRING_HPP