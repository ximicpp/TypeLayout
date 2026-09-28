#pragma once
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace dia_observer {
class PeImage {
    std::vector<unsigned char> bytes_;
    std::vector<IMAGE_SECTION_HEADER> sections_;
    DWORD headers_size_{};
    IMAGE_DATA_DIRECTORY directories_[IMAGE_NUMBEROF_DIRECTORY_ENTRIES]{};

    template<class T> T read(std::size_t offset) const {
        if (offset > bytes_.size() || sizeof(T) > bytes_.size() - offset)
            throw std::runtime_error("malformed-pe: out-of-bounds structure");
        T value{};
        std::memcpy(&value, bytes_.data() + offset, sizeof(T));
        return value;
    }
    std::size_t rva(DWORD address, std::size_t length) const {
        std::size_t result = address;
        if (address >= headers_size_) {
            bool found = false;
            for (const auto& section : sections_) {
                if (address >= section.VirtualAddress &&
                    static_cast<std::uint64_t>(address) - section.VirtualAddress < section.SizeOfRawData) {
                    const auto delta = address - section.VirtualAddress;
                    if (length > section.SizeOfRawData - delta) throw std::runtime_error("malformed-pe: RVA spans raw section");
                    result = static_cast<std::size_t>(section.PointerToRawData) + delta;
                    found = true; break;
                }
            }
            if (!found) throw std::runtime_error("malformed-pe: RVA does not map to file data");
        }
        if (result > bytes_.size() || length > bytes_.size() - result)
            throw std::runtime_error("malformed-pe: RVA exceeds file");
        return result;
    }
    std::string cstring_at(std::size_t offset, std::size_t limit = 4096) const {
        if (offset >= bytes_.size()) throw std::runtime_error("malformed-pe: string offset");
        const auto max = std::min(bytes_.size() - offset, limit);
        const auto* begin = reinterpret_cast<const char*>(bytes_.data() + offset);
        const auto* end = static_cast<const char*>(std::memchr(begin, 0, max));
        if (!end) throw std::runtime_error("malformed-pe: unterminated string");
        return {begin, end};
    }

public:
    WORD machine{};
    unsigned pointer_bits{};
    GUID guid{};
    DWORD age{};

    explicit PeImage(const std::filesystem::path& path) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) throw std::runtime_error("pe-not-found: " + path.string());
        bytes_ = {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        const auto dos = read<IMAGE_DOS_HEADER>(0);
        if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0) throw std::runtime_error("malformed-pe: DOS header");
        const auto nt = static_cast<std::size_t>(dos.e_lfanew);
        if (read<DWORD>(nt) != IMAGE_NT_SIGNATURE) throw std::runtime_error("malformed-pe: PE signature");
        const auto file = read<IMAGE_FILE_HEADER>(nt + sizeof(DWORD));
        machine = file.Machine;
        const auto optional_offset = nt + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
        const auto magic = read<WORD>(optional_offset);
        if (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            if (file.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64)) throw std::runtime_error("malformed-pe: optional header size");
            const auto optional = read<IMAGE_OPTIONAL_HEADER64>(optional_offset);
            if (optional.NumberOfRvaAndSizes < IMAGE_NUMBEROF_DIRECTORY_ENTRIES) throw std::runtime_error("malformed-pe: missing directories");
            pointer_bits = 64; headers_size_ = optional.SizeOfHeaders;
            std::copy(std::begin(optional.DataDirectory), std::end(optional.DataDirectory), directories_);
        } else if (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            if (file.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER32)) throw std::runtime_error("malformed-pe: optional header size");
            const auto optional = read<IMAGE_OPTIONAL_HEADER32>(optional_offset);
            if (optional.NumberOfRvaAndSizes < IMAGE_NUMBEROF_DIRECTORY_ENTRIES) throw std::runtime_error("malformed-pe: missing directories");
            pointer_bits = 32; headers_size_ = optional.SizeOfHeaders;
            std::copy(std::begin(optional.DataDirectory), std::end(optional.DataDirectory), directories_);
        } else throw std::runtime_error("unsupported-pe-optional-header");
        for (unsigned i = 0; i < file.NumberOfSections; ++i)
            sections_.push_back(read<IMAGE_SECTION_HEADER>(optional_offset + file.SizeOfOptionalHeader + i * sizeof(IMAGE_SECTION_HEADER)));
        const auto& debug = directories_[IMAGE_DIRECTORY_ENTRY_DEBUG];
        if (!debug.VirtualAddress || !debug.Size) throw std::runtime_error("missing-codeview: PE has no debug directory");
        if (debug.Size % sizeof(IMAGE_DEBUG_DIRECTORY)) throw std::runtime_error("malformed-pe: debug directory size");
        const auto debug_offset = rva(debug.VirtualAddress, debug.Size);
        bool found = false;
        for (unsigned i = 0; i < debug.Size / sizeof(IMAGE_DEBUG_DIRECTORY); ++i) {
            const auto entry = read<IMAGE_DEBUG_DIRECTORY>(debug_offset + i * sizeof(IMAGE_DEBUG_DIRECTORY));
            if (entry.Type != IMAGE_DEBUG_TYPE_CODEVIEW) continue;
            if (entry.SizeOfData < 24 || entry.PointerToRawData > bytes_.size() || entry.SizeOfData > bytes_.size() - entry.PointerToRawData)
                throw std::runtime_error("malformed-pe: CodeView payload");
            if (read<DWORD>(entry.PointerToRawData) != 0x53445352) continue; // RSDS
            const auto next_guid = read<GUID>(entry.PointerToRawData + 4);
            const auto next_age = read<DWORD>(entry.PointerToRawData + 20);
            if (found && (std::memcmp(&guid, &next_guid, sizeof(GUID)) || age != next_age))
                throw std::runtime_error("ambiguous-codeview: conflicting PDB identities");
            guid = next_guid; age = next_age; found = true;
        }
        if (!found) throw std::runtime_error("missing-codeview: no RSDS PDB identity");
    }

    std::optional<std::string> exported_string(const std::string& expected) const {
        const auto& export_dir = directories_[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!export_dir.VirtualAddress) return std::nullopt;
        const auto exports = read<IMAGE_EXPORT_DIRECTORY>(rva(export_dir.VirtualAddress, sizeof(IMAGE_EXPORT_DIRECTORY)));
        if (exports.NumberOfNames > bytes_.size() / 4 || exports.NumberOfFunctions > bytes_.size() / 4)
            throw std::runtime_error("malformed-pe: export counts");
        const auto names = rva(exports.AddressOfNames, exports.NumberOfNames * sizeof(DWORD));
        const auto ordinals = rva(exports.AddressOfNameOrdinals, exports.NumberOfNames * sizeof(WORD));
        const auto functions = rva(exports.AddressOfFunctions, exports.NumberOfFunctions * sizeof(DWORD));
        for (unsigned i = 0; i < exports.NumberOfNames; ++i) {
            const auto name_address = read<DWORD>(names + i * sizeof(DWORD));
            if (cstring_at(rva(name_address, 1)) != expected) continue;
            const auto ordinal = read<WORD>(ordinals + i * sizeof(WORD));
            if (ordinal >= exports.NumberOfFunctions) throw std::runtime_error("malformed-pe: export ordinal");
            const auto address = read<DWORD>(functions + ordinal * sizeof(DWORD));
            return cstring_at(rva(address, 1));
        }
        return std::nullopt;
    }
};
} // namespace dia_observer
