#pragma once

#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <locale>
#include <map>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <variant>
#include <vector>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace observer {

#define TYPELAYOUT_STRINGIFY_IMPL(x) #x
#define TYPELAYOUT_STRINGIFY(x) TYPELAYOUT_STRINGIFY_IMPL(x)
inline const char* compiler_version() {
#if defined(__clang_version__)
    return __clang_version__;
#elif defined(_MSC_FULL_VER)
    return "MSVC " TYPELAYOUT_STRINGIFY(_MSC_FULL_VER);
#elif defined(__VERSION__)
    return __VERSION__;
#else
    return "unknown";
#endif
}

// Small write-only JSON value: no parser, external dependency, or locale-sensitive numbers.
class Json {
public:
    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json>;
    using Value = std::variant<std::nullptr_t, bool, std::int64_t, std::string, Array, Object>;
    Value value;

    Json() : value(nullptr) {}
    Json(bool x) : value(x) {}
    Json(int x) : value(static_cast<std::int64_t>(x)) {}
    Json(std::int64_t x) : value(x) {}
    Json(const char* x) : value(std::string(x)) {}
    Json(std::string x) : value(std::move(x)) {}
    Json(Array x) : value(std::move(x)) {}
    Json(Object x) : value(std::move(x)) {}
    static Json object(std::initializer_list<Object::value_type> x) { return Object(x); }
    static Json array(std::initializer_list<Json> x = {}) { return Array(x); }
    Json& operator[](const std::string& key) { return std::get<Object>(value)[key]; }
    const Json& at(const std::string& key) const { return std::get<Object>(value).at(key); }
    Array& array_items() { return std::get<Array>(value); }
    const Array& array_items() const { return std::get<Array>(value); }

    static void quote(std::ostream& out, const std::string& text) {
        constexpr char hex[] = "0123456789abcdef";
        out << '"';
        for (unsigned char c : text) {
            switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) out << "\\u00" << hex[c >> 4] << hex[c & 15];
                else out << static_cast<char>(c);
            }
        }
        out << '"';
    }
    void write(std::ostream& out) const {
        std::visit([&](const auto& x) {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) out << "null";
            else if constexpr (std::is_same_v<T, bool>) out << (x ? "true" : "false");
            else if constexpr (std::is_same_v<T, std::string>) quote(out, x);
            else if constexpr (std::is_same_v<T, Array>) {
                out << '[';
                bool comma = false;
                for (const auto& item : x) { if (comma) out << ','; item.write(out); comma = true; }
                out << ']';
            } else if constexpr (std::is_same_v<T, Object>) {
                out << '{';
                bool comma = false;
                for (const auto& [key, item] : x) {
                    if (comma) out << ',';
                    quote(out, key); out << ':'; item.write(out); comma = true;
                }
                out << '}';
            } else out << x;
        }, value);
    }
};

// Kernel-enforced exclusive creation. Never use exists()+ofstream here: that
// leaves a race in which another capture or a baseline can be truncated.
inline void write_new_snapshot(const std::filesystem::path& path, const Json& snapshot) {
    std::ostringstream encoded;
    encoded.imbue(std::locale::classic());
    snapshot.write(encoded); encoded << '\n';
    if (!encoded) throw std::runtime_error("snapshot-serialization-failed");
    const auto payload = encoded.str();
    std::size_t offset = 0;
#if defined(_WIN32)
    const auto file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "exclusive-output-create-failed");
    while (offset < payload.size()) {
        const auto remaining = payload.size() - offset;
        const auto count = static_cast<DWORD>(remaining > 1048576 ? 1048576 : remaining);
        DWORD written{};
        if (!WriteFile(file, payload.data() + offset, count, &written, nullptr) || written == 0) {
            const auto error = GetLastError();
            CloseHandle(file);
            throw std::system_error(static_cast<int>(error ? error : ERROR_WRITE_FAULT), std::system_category(), "snapshot-write-failed");
        }
        offset += written;
    }
    if (!CloseHandle(file))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "snapshot-close-failed");
#else
    const auto file = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
    if (file < 0) throw std::system_error(errno, std::generic_category(), "exclusive-output-create-failed");
    while (offset < payload.size()) {
        const auto remaining = payload.size() - offset;
        const auto count = remaining > 1048576 ? 1048576 : remaining;
        const auto written = ::write(file, payload.data() + offset, count);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) {
            const auto error = written < 0 ? errno : EIO;
            ::close(file);
            throw std::system_error(error, std::generic_category(), "snapshot-write-failed");
        }
        offset += static_cast<std::size_t>(written);
    }
    // A close error is reported, not retried on a possibly already-closed fd.
    if (::close(file) != 0) throw std::system_error(errno, std::generic_category(), "snapshot-close-failed");
#endif
}

inline Json known(Json value, const std::string& method, const std::string& kind = "compiler") {
    return Json::object({{"state", "known"}, {"value", std::move(value)},
        {"evidence", Json::object({{"kind", kind}, {"method", method},
            {"version", compiler_version()}, {"inputs", Json::array()}})}});
}
inline Json number(std::size_t n, const std::string& method) {
    return known(static_cast<std::int64_t>(n), method);
}
inline Json unknown(const std::string& reason) {
    return Json::object({{"state", "unknown"}, {"reason", reason}});
}
inline Json not_applicable(const std::string& reason) {
    return Json::object({{"state", "not-applicable"}, {"reason", reason}});
}
inline Json range(std::size_t offset, std::size_t width, const std::string& method) {
    return known(Json::array({Json::object({{"startBit", static_cast<std::int64_t>(offset)},
        {"lengthBits", static_cast<std::int64_t>(width)}})}), method);
}

} // namespace observer
