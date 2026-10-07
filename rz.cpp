#include "rz.h"
#include <zlib.h>
#include <stdexcept>
#include <fstream>
#include <iostream>

namespace RcfRz {

std::vector<std::uint8_t> DecompressBuffer(const std::vector<std::uint8_t>& buf) {
    if (buf.size() < 16) {
        throw std::runtime_error(".rz file too small");
    }
    if (buf[0] != 'R' || buf[1] != 'Z') {
        throw std::runtime_error(".rz file does not start with RZ magic");
    }
    // Payload starts at 0x10 based on hex inspection of P2 .p3d.rz files.
    const std::uint8_t* in = buf.data() + 0x10;
    const std::size_t inLen = buf.size() - 0x10;

    z_stream z = z_stream();
    z.zalloc = Z_NULL;
    z.zfree = Z_NULL;
    z.opaque = Z_NULL;
    z.next_in = const_cast<Bytef*>(in);
    z.avail_in = static_cast<uInt>(inLen);
    z.next_out = nullptr;
    z.avail_out = 0;

    const int ret = inflateInit(&z); // defaults: -15 for zlib
    if (ret != Z_OK) {
        throw std::runtime_error("zlib inflateInit failed");
    }

    std::vector<std::uint8_t> out;
    std::uint8_t chunk[8192];
    int zret = Z_OK;
    do {
        z.next_out = chunk;
        z.avail_out = sizeof(chunk);
        zret = inflate(&z, Z_FINISH);
        if (zret == Z_STREAM_END || zret == Z_OK || zret == Z_BUF_ERROR) {
            std::size_t have = sizeof(chunk) - z.avail_out;
            out.insert(out.end(), chunk, chunk + have);
        } else if (zret == Z_DATA_ERROR || zret == Z_MEM_ERROR || zret == Z_NEED_DICT) {
            inflateEnd(&z);
            throw std::runtime_error("zlib inflate error (code " + std::to_string(zret) + ")");
        }
    } while (zret == Z_OK || zret == Z_BUF_ERROR);

    inflateEnd(&z);
    return out;
}

std::vector<std::uint8_t> DecompressFile(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open .rz: " + file.string());
    }
    in.seekg(0, std::ios::end);
    const auto sz = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(sz));
    if (!buf.empty()) {
        in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    }
    return DecompressBuffer(buf);
}

void ExtractRz(const std::filesystem::path& file,
               const std::filesystem::path& outDir) {
    const std::string name = file.filename().string();
    std::filesystem::path rel = name;
    if (name.find(".p3d.rz") != std::string::npos) {
        std::string plain = name;
        const std::string suffix = ".p3d.rz";
        if (plain.size() >= suffix.size() &&
            plain.substr(plain.size() - suffix.size()) == suffix) {
            plain.resize(plain.size() - suffix.size());
            rel = plain;
        }
    }
    const std::vector<std::uint8_t> data = DecompressFile(file);
    const std::filesystem::path outPath = outDir / rel;
    std::filesystem::create_directories(outPath.parent_path());
    std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
    if (!data.empty()) {
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    std::cout << "decompressed .rz: " << file.filename().string()
              << " -> " << outPath.string()
              << " (" << data.size() << " bytes)\n";
}

} // namespace RcfRz
