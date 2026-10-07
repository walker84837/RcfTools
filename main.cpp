// RcfTools: inspect, extract and rebuild Prototype 2 .rcf archives.
//
// Usage:
//   RcfTools info <archive.rcf>
//   RcfTools list <archive.rcf>
//   RcfTools extract <archive.rcf> <path|hash|index> [-o outdir]
//   RcfTools extract-all <archive.rcf> [-o outdir]
//   RcfTools verify <archive.rcf>
//   RcfTools hash <path>...
//   RcfTools pack <srcdir> <out.rcf>   (experimental)

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include <cxxopts.hpp>

#include "rcf_file.h"
#include "rcf_hash.h"

namespace {

void PrintMember(const RcfMember& m, std::size_t i) {
    std::printf("%4zu  %08X  %10u  %s\n", i, m.hash, m.size,
                m.name.empty() ? "(no name)" : m.name.c_str());
}

int CmdInfo(const RcfArchive& a) {
    const auto& h = a.header();
    std::printf("file:     %s\n", a.path().string().c_str());
    std::printf("magic:    %s\n", h.name.c_str());
    std::printf("version:  0x%08X\n", h.version);
    std::printf("members:  %u\n", h.numberOfFiles);
    std::printf("entries:  off=0x%X len=%u\n", h.entryOffset, h.entryLength);
    std::printf("metadata: off=0x%X len=%u\n", h.metadataOffset,
                h.metadataLength);
    return 0;
}

int CmdList(const RcfArchive& a) {
    std::printf(" idx  hash      size       name\n");
    const auto& ms = a.members();
    for (std::size_t i = 0; i < ms.size(); ++i) {
        PrintMember(ms[i], i);
    }
    return 0;
}

std::optional<std::size_t> ResolveTarget(const RcfArchive& a,
                                         const std::string& target) {
    // By index.
    try {
        std::size_t pos = 0;
        const auto idx = std::stoul(target, &pos);
        if (pos == target.size() && idx < a.members().size()) {
            return idx;
        }
    } catch (...) {
    }
    // By hash (hex, with or without 0x).
    try {
        std::size_t pos = 0;
        const auto h = std::stoul(target, &pos, 16);
        if (pos == target.size() ||
            (target.size() > 2 && target[0] == '0' &&
             (target[1] == 'x' || target[1] == 'X'))) {
            return a.findByHash(static_cast<std::uint32_t>(h));
        }
    } catch (...) {
    }
    // By path.
    return a.findByPath(target);
}

int CmdExtract(const RcfArchive& a, const std::string& target,
               const std::filesystem::path& outDir) {
    const auto idx = ResolveTarget(a, target);
    if (!idx) {
        std::cerr << "error: no member matches '" << target << "'\n";
        return 1;
    }
    const auto dest = a.extract(*idx, outDir);
    const auto& m = a.members()[*idx];
    std::printf("extracted %08X (%u bytes) -> %s\n", m.hash, m.size,
                dest.string().c_str());
    return 0;
}

int CmdExtractAll(const RcfArchive& a, const std::filesystem::path& outDir) {
    const auto n = a.extractAll(outDir);
    std::printf("extracted %zu files -> %s\n", n, outDir.string().c_str());
    return 0;
}

int CmdVerify(const RcfArchive& a) {
    const auto problems = a.verify();
    if (problems.empty()) {
        std::printf("OK: %s (%zu members)\n", a.path().string().c_str(),
                    a.members().size());
        return 0;
    }
    std::printf("FAIL: %s\n", a.path().string().c_str());
    for (const auto& p : problems) {
        std::printf("  - %s\n", p.c_str());
    }
    return 1;
}

int CmdHash(const std::vector<std::string>& paths) {
    for (const auto& p : paths) {
        std::printf("%08X  %s\n",
                    RadicalHash(NormaliseRcfPath(p)), p.c_str());
    }
    return 0;
}

void PrintUsage() {
    std::cout
        << "Usage: RcfTools <command> [args]\n"
           "  info <archive.rcf>                 show header summary\n"
           "  list <archive.rcf>                 list members\n"
           "  extract <archive.rcf> <t> [-o d]   extract one member by "
           "index, hash or path\n"
           "  extract-all <archive.rcf> [-o d]   extract all members\n"
           "  verify <archive.rcf>               structural checks\n"
           "  hash <path>...                     print Radical hash(es)\n"
           "  pack <srcdir> <out.rcf>            rebuild archive "
           "(experimental)\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        PrintUsage();
        return 2;
    }
    const std::string cmd = argv[1];
    try {
        if (cmd == "hash") {
            if (argc < 3) {
                std::cerr << "error: hash needs at least one path\n";
                return 2;
            }
            return CmdHash({argv + 2, argv + argc});
        }
        if (cmd == "pack") {
            if (argc != 4) {
                std::cerr << "error: pack needs <srcdir> <out.rcf>\n";
                return 2;
            }
            RcfArchive::pack(argv[2], argv[3]);
            return 0;
        }
        if (cmd == "info" || cmd == "list" || cmd == "verify" ||
            cmd == "extract" || cmd == "extract-all") {
            if (argc < 3) {
                std::cerr << "error: " << cmd << " needs <archive.rcf>\n";
                return 2;
            }
            RcfArchive a(argv[2]);
            if (cmd == "info") {
                return CmdInfo(a);
            }
            if (cmd == "list") {
                return CmdList(a);
            }
            if (cmd == "verify") {
                return CmdVerify(a);
            }
            std::filesystem::path outDir = "extracted";
            for (int i = 3; i + 1 < argc; ++i) {
                if (std::string(argv[i]) == "-o") {
                    outDir = argv[i + 1];
                }
            }
            if (cmd == "extract-all") {
                return CmdExtractAll(a, outDir);
            }
            if (argc < 4) {
                std::cerr << "error: extract needs a member target\n";
                return 2;
            }
            return CmdExtract(a, argv[3], outDir);
        }
        std::cerr << "error: unknown command '" << cmd << "'\n";
        PrintUsage();
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
