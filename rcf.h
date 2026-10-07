#pragma once
// RCF (Cement Library) on-disk format, Prototype 2 (ATG) variant.
// SHAR variant notes included where the layout differs.
//
// Two known variants:
//   Prototype 2 : magic "ATG CORE CEMENT LIBRARY", version 0x01000102,
//                 directory packed right after the 60-byte header.
//   SHAR        : magic "RADCORE CEMENT LIBRARY", version 0x01000201,
//                 directory page-aligned at 0x800.
// Both store the directory as a sorted array of {hash, offset, size}
// (12 bytes each), so lookup is a binary search. Prototype 2 additionally
// stores member paths in a metadata table; SHAR does not.

#include <cstdint>
#include <string>
#include <vector>

inline constexpr std::size_t kRcfHeaderLength = 60;
inline constexpr std::size_t kRcfEntrySize = 12;

// Magic strings (22 bytes, zero-padded to 32).
inline constexpr char kMagicAtg[] = "ATG CORE CEMENT LIBRARY";    // Prototype 2
inline constexpr char kMagicRadcore[] = "RADCORE CEMENT LIBRARY"; // SHAR

// Known version words (little-endian u32 at 0x20).
inline constexpr std::uint32_t kVersionPrototype2 = 0x01000102;
inline constexpr std::uint32_t kVersionShar = 0x01000201;

enum class RcfHeaderOffsets : std::uint8_t {
    Name = 0x00,        // char[32]
    Version = 0x20,     // u32
    EndianFlag = 0x22,  // u8 (0 = little)
    LibraryValid = 0x23,// u8 (must be 1)
    EntryOffset = 0x24, // u32: absolute offset of the entry directory
    EntryLength = 0x28, // u32: bytes covered by entries (count * 12)
    MetadataOffset = 0x2C,// u32: absolute offset of the metadata table
    MetadataLength = 0x30,// u32: bytes covered by metadata
    EmptySpace = 0x34,  // u32: reserved, 0
    NumberOfFiles = 0x38,// u32
};

struct RcfHeader {
    std::string name;
    std::uint32_t version = 0;
    bool bigEndian = false;
    bool libraryValid = false;
    std::uint32_t entryOffset = 0;
    std::uint32_t entryLength = 0;
    std::uint32_t metadataOffset = 0;
    std::uint32_t metadataLength = 0;
    std::uint32_t numberOfFiles = 0;
};

enum class RcfEntryOffsets : std::uint8_t {
    Hash = 0x0,   // u32: Radical hash of the member path
    Offset = 0x4, // u32: absolute offset of member data
    Length = 0x8, // u32: member size in bytes
};

struct RcfEntry {
    std::uint32_t hash = 0;
    std::uint32_t dataOffset = 0;
    std::uint32_t dataLength = 0;
};

// Prototype 2 metadata table layout:
//   +0x00: 8-byte global preamble (u32 0x800, u32 0)
//   per file record:
//     +0x00: u32 date (0 in shipped file)
//     +0x04: u8 padding[4] = {0x00, 0x08, 0x00, 0x00}
//     +0x08: u32 reserved (0)
//     +0x0C: u32 filenameLength (INCLUDES the NUL terminator)
//     +0x10: char filename[filenameLength] (NUL-terminated)
//     followed by 3 zero pad bytes.
//   Record stride = 0x10 + filenameLength + 3.
// The first record starts at metadataOffset + 0x08.
inline constexpr std::uint32_t kMetadataPreambleSize = 8;
inline constexpr std::uint32_t kMetadataRecordHeader = 0x10;
inline constexpr std::uint32_t kMetadataRecordTailPad = 3;

struct RcfMetadata {
    std::uint32_t date = 0;
    std::uint8_t padding[4] = {0x00, 0x08, 0x00, 0x00};
    std::uint32_t reserved = 0;
    std::uint32_t filenameLength = 0;
    std::string filename; // without trailing NUL
};
