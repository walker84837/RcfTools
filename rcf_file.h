#pragma once
// RcfArchive: read (and experimentally write) Prototype 2 Cement Library
// (.rcf) archives.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "rcf.h"

struct RcfMember {
    std::uint32_t hash = 0;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
    std::string name; // empty when the metadata table has no match
};

class RcfArchive {
  public:
    RcfArchive() = default;
    explicit RcfArchive(const std::filesystem::path& file);

    void load(const std::filesystem::path& file);

    [[nodiscard]] const RcfHeader& header() const noexcept { return header_; }
    [[nodiscard]] const std::vector<RcfMember>& members() const noexcept {
        return members_;
    }
    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

    // Binary search over the hash-sorted directory (entries are stored
    // ascending by hash; the engine relies on this).
    [[nodiscard]] std::optional<std::size_t> findByHash(
        std::uint32_t hash) const noexcept;
    [[nodiscard]] std::optional<std::size_t> findByPath(
        const std::string& path) const;

    // Raw member bytes.
    [[nodiscard]] std::vector<std::uint8_t> readMember(
        std::size_t index) const;

    // Extract member `index` under `outDir` (creates parent dirs).
    // Returns the written path.
    std::filesystem::path extract(std::size_t index,
                                  const std::filesystem::path& outDir) const;
    // Extract every member. Returns number of files written.
    std::size_t extractAll(const std::filesystem::path& outDir) const;

    // Structural checks: magic, version, entry bounds, hash sort order,
    // metadata/entry cross-references. Returns list of problems (empty = ok).
    [[nodiscard]] std::vector<std::string> verify() const;

    // EXPERIMENTAL: pack `srcDir` (recursively) into `outFile`.
    static void pack(const std::filesystem::path& srcDir,
                     const std::filesystem::path& outFile);

  private:
    std::filesystem::path path_;
    std::vector<std::uint8_t> image_;
    RcfHeader header_;
    std::vector<RcfMember> members_;

    [[nodiscard]] std::vector<std::uint8_t> slice(std::uint32_t from,
                                                  std::uint32_t length) const;
    void parseHeader();
    void parseEntries();
    void parseMetadata();
};
