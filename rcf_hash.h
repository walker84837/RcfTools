#pragma once
// Prototype 2 Radical path hash.
//
// Ground truth from enable_debug.rcf:
//   hash("art\\hud\\joined_fe_backpause.p3d.rz") == 0x1ABDE27C
//   hash("scripts\\datainit.lua")                == 0xF5CEE618
//
// Algorithm: res = res * 31 + fold(c) for each byte, 32-bit wrap.
// fold(c) = c + 32 for c < 'a', else c. (This folds 'A'-'Z' to
// 'a'-'z' but also shifts digits/punctuation/slashes by +32; that is
// the shipped behaviour, not a bug to "fix".)
// A single leading backslash is skipped when it is the first character.
//
// NOTE: this differs from the HnREncyclopedia C2.2 reconstruction
// (plain .lower() + multiply-add), which does NOT reproduce P2 keys.
// That reference documents Simpsons Hit & Run; the function below is the
// Prototype 2 variant.

#include <cstdint>
#include <string>
#include <string_view>

[[nodiscard]] inline std::uint32_t RadicalHash(std::string_view path) noexcept {
    std::uint32_t res = 0;
    for (char ch : path) {
        if (res == 0 && ch == '\\') {
            continue; // skip one leading backslash
        }
        const auto u = static_cast<unsigned char>(ch);
        const std::uint32_t c =
            (u < static_cast<unsigned char>('a')) ? (u + 32u) : u;
        res = (res << 5) - res + c; // res * 31 + c, wraps mod 2^32
    }
    return res;
}

// Normalise a user-supplied path to engine form: forward slashes to
// backslashes. (Case folding happens inside RadicalHash itself.)
[[nodiscard]] inline std::string NormaliseRcfPath(std::string_view path) {
    std::string out(path);
    for (char& ch : out) {
        if (ch == '/') {
            ch = '\\';
        }
    }
    return out;
}
