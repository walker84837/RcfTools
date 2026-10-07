#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Prototype 2 .p3d.rz — a compressed Pure3D container.
//
// Layout (verified on art/hud/joined_fe_backpause.p3d.rz):
//   0x00:  "RZ" (2 bytes)
//   0x02:  14-byte header (zeros + size fields; exact semantics open,
//         but the zlib stream starts reliably at 0x10).
//   0x10:  zlib-compressed payload.
//
// Decompression yields a standard P3D\xff file (verified: decompressed
// header is P3D\xff 0x0c, size matches file header at 0x08).

namespace RcfRz {

// Decompress an .rz file to memory. Returns the uncompressed bytes.
// Throws std::runtime_error on failure.
std::vector<std::uint8_t> DecompressFile(const std::filesystem::path& file);

// Same from an in-memory .rz buffer.
std::vector<std::uint8_t> DecompressBuffer(const std::vector<std::uint8_t>& buf);

// Extract one .rz file to <outDir> / basename (decompresses in place).
void ExtractRz(const std::filesystem::path& file,
               const std::filesystem::path& outDir);

} // namespace RcfRz
