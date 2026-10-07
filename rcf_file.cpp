// RcfArchive implementation. Format notes live in rcf.h.

#include "rcf_file.h"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "rcf_hash.h"
#include "utils.h"

namespace {

std::vector<std::uint8_t> ReadWholeFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open file: " + path.string());
    }
    in.seekg(0, std::ios::end);
    const auto size = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(size));
    if (!buf.empty()) {
        in.read(reinterpret_cast<char*>(buf.data()),
                static_cast<std::streamsize>(buf.size()));
    }
    return buf;
}

void WriteWholeFile(const std::filesystem::path& path,
                    const std::vector<std::uint8_t>& buf) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("cannot write file: " + path.string());
    }
    if (!buf.empty()) {
        out.write(reinterpret_cast<const char*>(buf.data()),
                  static_cast<std::streamsize>(buf.size()));
    }
}

} // namespace

RcfArchive::RcfArchive(const std::filesystem::path& file) { load(file); }

void RcfArchive::load(const std::filesystem::path& file) {
    path_ = file;
    image_ = ReadWholeFile(file);
    members_.clear();
    parseHeader();
    parseEntries();
    parseMetadata();
}

std::vector<std::uint8_t> RcfArchive::slice(std::uint32_t from,
                                            std::uint32_t length) const {
    if (static_cast<std::uint64_t>(from) + length > image_.size()) {
        throw std::runtime_error("archive truncated: slice out of range");
    }
    return {image_.begin() + from, image_.begin() + from + length};
}

void RcfArchive::parseHeader() {
    if (image_.size() < kRcfHeaderLength) {
        throw std::runtime_error("file too small to be an RCF archive");
    }
    const auto* d = image_.data();

    std::string name(reinterpret_cast<const char*>(d), 32);
    if (auto nul = name.find('\0'); nul != std::string::npos) {
        name.resize(nul);
    }
    const bool isAtg = name == kMagicAtg;
    const bool isRadcore = name == kMagicRadcore;
    if (!isAtg && !isRadcore) {
        throw std::runtime_error("bad RCF magic: '" + name + "'");
    }

    header_.name = name;
    header_.version = ReadU32LE(d, EnumToValue(RcfHeaderOffsets::Version));
    header_.bigEndian = d[EnumToValue(RcfHeaderOffsets::EndianFlag)] != 0;
    header_.libraryValid =
        d[EnumToValue(RcfHeaderOffsets::LibraryValid)] != 0;
    header_.entryOffset = ReadU32LE(d, EnumToValue(RcfHeaderOffsets::EntryOffset));
    header_.entryLength = ReadU32LE(d, EnumToValue(RcfHeaderOffsets::EntryLength));
    header_.metadataOffset =
        ReadU32LE(d, EnumToValue(RcfHeaderOffsets::MetadataOffset));
    header_.metadataLength =
        ReadU32LE(d, EnumToValue(RcfHeaderOffsets::MetadataLength));
    header_.numberOfFiles =
        ReadU32LE(d, EnumToValue(RcfHeaderOffsets::NumberOfFiles));

    if (header_.bigEndian) {
        throw std::runtime_error("big-endian RCF archives are not supported");
    }
    if (!header_.libraryValid) {
        throw std::runtime_error("RCF library-valid flag is not set");
    }
    if (header_.version != kVersionPrototype2 &&
        header_.version != kVersionShar) {
        std::cerr << "warning: unknown RCF version 0x" << std::hex
                  << header_.version << std::dec << "; attempting parse\n";
    }
}

void RcfArchive::parseEntries() {
    const std::uint64_t count = header_.numberOfFiles;
    if (count * kRcfEntrySize != header_.entryLength) {
        throw std::runtime_error("entry length does not match file count");
    }
    if (static_cast<std::uint64_t>(header_.entryOffset) +
            header_.entryLength >
        image_.size()) {
        throw std::runtime_error("entry directory out of range");
    }
    const auto* d = image_.data();
    members_.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) {
        const std::uint32_t base = header_.entryOffset +
                                   static_cast<std::uint32_t>(i * kRcfEntrySize);
        RcfMember m;
        m.hash = ReadU32LE(d, base + EnumToValue(RcfEntryOffsets::Hash));
        m.offset = ReadU32LE(d, base + EnumToValue(RcfEntryOffsets::Offset));
        m.size = ReadU32LE(d, base + EnumToValue(RcfEntryOffsets::Length));
        if (static_cast<std::uint64_t>(m.offset) + m.size > image_.size()) {
            throw std::runtime_error("member data out of range");
        }
        members_.push_back(m);
    }
}

void RcfArchive::parseMetadata() {
    if (header_.numberOfFiles == 0 || header_.metadataLength == 0) {
        return; // SHAR-style archives carry no names; hashes only.
    }
    if (static_cast<std::uint64_t>(header_.metadataOffset) +
            header_.metadataLength >
        image_.size()) {
        throw std::runtime_error("metadata table out of range");
    }
    const auto* d = image_.data();
    std::uint32_t off = header_.metadataOffset + kMetadataPreambleSize;
    const std::uint32_t end = header_.metadataOffset + header_.metadataLength;

    // Name every member whose hash matches (linear; tables are tiny).
    for (std::uint64_t i = 0;
         i < header_.numberOfFiles && off + kMetadataRecordHeader <= end;
         ++i) {
        const std::uint32_t date = ReadU32LE(d, off + 0x00);
        (void)date;
        const std::uint32_t len = ReadU32LE(d, off + 0x0C);
        if (len == 0 || off + kMetadataRecordHeader + len > end) {
            break;
        }
        std::string name(reinterpret_cast<const char*>(d + off + 0x10), len);
        if (!name.empty() && name.back() == '\0') {
            name.pop_back();
        }
        const std::uint32_t h =
            RadicalHash(NormaliseRcfPath(name));
        for (auto& m : members_) {
            if (m.hash == h && m.name.empty()) {
                m.name = name;
                break;
            }
        }
        off += kMetadataRecordHeader + len + kMetadataRecordTailPad;
    }
}

std::optional<std::size_t> RcfArchive::findByHash(
    std::uint32_t hash) const noexcept {
    std::size_t lo = 0, hi = members_.size();
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        if (members_[mid].hash == hash) {
            return mid;
        }
        if (members_[mid].hash < hash) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> RcfArchive::findByPath(
    const std::string& path) const {
    return findByHash(RadicalHash(NormaliseRcfPath(path)));
}

std::vector<std::uint8_t> RcfArchive::readMember(std::size_t index) const {
    if (index >= members_.size()) {
        throw std::out_of_range("member index out of range");
    }
    const auto& m = members_[index];
    return slice(m.offset, m.size);
}

std::filesystem::path RcfArchive::extract(
    std::size_t index, const std::filesystem::path& outDir) const {
    if (index >= members_.size()) {
        throw std::out_of_range("member index out of range");
    }
    const auto& m = members_[index];
    // Engine paths use '\'; map to the platform separator on extraction.
    std::string portable = m.name;
    if (!portable.empty()) {
        for (char& ch : portable) {
            if (ch == '\\') {
                ch = static_cast<char>(std::filesystem::path::preferred_separator);
            }
        }
    }
    std::filesystem::path rel;
    if (!portable.empty()) {
        rel = portable;
    } else {
        char buf[16];
        std::snprintf(buf, sizeof buf, "%08X.bin", m.hash);
        rel = buf;
    }
    const std::filesystem::path dest = outDir / rel;
    std::filesystem::create_directories(dest.parent_path());
    WriteWholeFile(dest, slice(m.offset, m.size));
    return dest;
}

std::size_t RcfArchive::extractAll(const std::filesystem::path& outDir) const {
    for (std::size_t i = 0; i < members_.size(); ++i) {
        extract(i, outDir);
    }
    return members_.size();
}

std::vector<std::string> RcfArchive::verify() const {
    std::vector<std::string> problems;
    if (header_.name != kMagicAtg && header_.name != kMagicRadcore) {
        problems.push_back("unknown magic: '" + header_.name + "'");
    }
    if (!header_.libraryValid) {
        problems.push_back("library-valid flag not set");
    }
    if (header_.numberOfFiles * kRcfEntrySize != header_.entryLength) {
        problems.push_back("entryLength != numberOfFiles * 12");
    }
    for (std::size_t i = 1; i < members_.size(); ++i) {
        if (members_[i].hash < members_[i - 1].hash) {
            problems.push_back("directory not sorted by hash at index " +
                                std::to_string(i));
            break;
        }
    }
    for (std::size_t i = 0; i < members_.size(); ++i) {
        const auto& m = members_[i];
        if (static_cast<std::uint64_t>(m.offset) + m.size > image_.size()) {
            problems.push_back("member " + std::to_string(i) +
                                " data out of range");
        }
        if (m.name.empty()) {
            char buf[64];
            std::snprintf(buf, sizeof buf,
                          "member %zu (hash %08X) has no name", i, m.hash);
            problems.push_back(buf);
        }
    }
    return problems;
}

// --- Packing (EXPERIMENTAL) --------------------------------------------
// Rebuilds a Prototype 2-style archive: 60-byte header, entry directory
// immediately after, metadata table page-aligned at 0x800, member data
// page-aligned at 0x1000. Directory is sorted by hash.

namespace {

constexpr std::uint32_t kPageAlign = 2048;

void PadTo(std::vector<std::uint8_t>& out, std::uint32_t align) {
    while (out.size() % align != 0) {
        out.push_back(0);
    }
}

struct PackItem {
    std::uint32_t hash = 0;
    std::string name;
    std::vector<std::uint8_t> data;
};

} // namespace

void RcfArchive::pack(const std::filesystem::path& srcDir,
                      const std::filesystem::path& outFile) {
    if (!std::filesystem::is_directory(srcDir)) {
        throw std::runtime_error("source is not a directory: " +
                                 srcDir.string());
    }
    std::vector<PackItem> items;
    for (const auto& e :
         std::filesystem::recursive_directory_iterator(srcDir)) {
        if (!e.is_regular_file()) {
            continue;
        }
        std::error_code ec;
        std::string rel =
            std::filesystem::relative(e.path(), srcDir, ec).string();
        if (ec) {
            throw std::runtime_error("cannot relativise path: " +
                                     e.path().string());
        }
        rel = NormaliseRcfPath(rel);
        PackItem it;
        it.name = rel;
        it.hash = RadicalHash(rel);
        it.data = ReadWholeFile(e.path());
        items.push_back(std::move(it));
    }
    std::sort(items.begin(), items.end(), [](const PackItem& a,
                                             const PackItem& b) {
        return a.hash < b.hash;
    });
    for (std::size_t i = 1; i < items.size(); ++i) {
        if (items[i].hash == items[i - 1].hash) {
            throw std::runtime_error("hash collision between '" +
                                     items[i - 1].name + "' and '" +
                                     items[i].name + "'");
        }
    }

    std::vector<std::uint8_t> out;
    out.reserve(1 << 20);

    // Header (filled in as offsets become known).
    {
        const std::string magic = kMagicAtg;
        out.insert(out.end(), magic.begin(), magic.end());
        out.resize(32, 0);
        AppendU32LE(out, kVersionPrototype2);
        AppendU32LE(out, 0); // entryOffset (patched below)
        AppendU32LE(out, 0); // entryLength (patched below)
        AppendU32LE(out, 0); // metadataOffset (patched below)
        AppendU32LE(out, 0); // metadataLength (patched below)
        AppendU32LE(out, 0); // reserved
        AppendU32LE(out, static_cast<std::uint32_t>(items.size()));
    }
    auto patchU32 = [&out](std::size_t at, std::uint32_t v) {
        out[at] = static_cast<std::uint8_t>(v & 0xFF);
        out[at + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
        out[at + 2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
        out[at + 3] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
    };
    // Endian flag (0) + library-valid (1).
    out[EnumToValue(RcfHeaderOffsets::EndianFlag)] = 0;
    out[EnumToValue(RcfHeaderOffsets::LibraryValid)] = 1;

    // Entry directory (hash, offset-placeholder, size).
    const std::uint32_t entryOffset = static_cast<std::uint32_t>(out.size());
    std::vector<std::size_t> offsetPatchAt;
    for (const auto& it : items) {
        AppendU32LE(out, it.hash);
        offsetPatchAt.push_back(out.size());
        AppendU32LE(out, 0);
        AppendU32LE(out, static_cast<std::uint32_t>(it.data.size()));
    }
    patchU32(EnumToValue(RcfHeaderOffsets::EntryOffset), entryOffset);
    patchU32(EnumToValue(RcfHeaderOffsets::EntryLength),
             static_cast<std::uint32_t>(items.size() * kRcfEntrySize));

    // Metadata table.
    PadTo(out, kPageAlign);
    const std::uint32_t metaOffset = static_cast<std::uint32_t>(out.size());
    {
        AppendU32LE(out, 0x800);
        AppendU32LE(out, 0);
    }
    for (const auto& it : items) {
        AppendU32LE(out, static_cast<std::uint32_t>(std::time(nullptr)));
        out.insert(out.end(), {0x00, 0x08, 0x00, 0x00});
        AppendU32LE(out, 0);
        const std::uint32_t len = static_cast<std::uint32_t>(it.name.size() + 1);
        AppendU32LE(out, len);
        out.insert(out.end(), it.name.begin(), it.name.end());
        out.push_back(0);
        out.insert(out.end(), 3, 0);
    }
    patchU32(EnumToValue(RcfHeaderOffsets::MetadataOffset), metaOffset);
    patchU32(EnumToValue(RcfHeaderOffsets::MetadataLength),
             static_cast<std::uint32_t>(out.size() - metaOffset));

    // Member data.
    PadTo(out, 0x1000);
    for (std::size_t i = 0; i < items.size(); ++i) {
        patchU32(offsetPatchAt[i], static_cast<std::uint32_t>(out.size()));
        out.insert(out.end(), items[i].data.begin(), items[i].data.end());
        if (i + 1 < items.size()) {
            PadTo(out, kPageAlign);
        }
    }

    WriteWholeFile(outFile, out);
    std::cout << "packed " << items.size() << " files -> " << outFile.string()
              << " (" << out.size() << " bytes)\n";
}
