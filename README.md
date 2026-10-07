# RcfTools

Inspect, extract and rebuild Prototype 2 `.rcf` (Cement Library) archives.
C++20, CMake, no third-party dependencies.

## Usage

```
RcfTools info <archive.rcf>
RcfTools list <archive.rcf>
RcfTools extract <archive.rcf> <index|hash|path> [-o outdir]
RcfTools extract-all <archive.rcf> [-o outdir]
RcfTools verify <archive.rcf>
RcfTools hash <path>...
RcfTools pack <srcdir> <out.rcf>   (experimental)
```

## Format

Header (60 bytes): `ATG CORE CEMENT LIBRARY` magic (32 bytes, NUL-padded),
u32 version (`0x01000102`), u8 endian flag, u8 library-valid flag, then u32
`entryOffset`, `entryLength`, `metadataOffset`, `metadataLength`, u32 reserved,
u32 file count.

Directory: flat array of `{u32 hash, u32 offset, u32 size}` (12 bytes each),
sorted ascending by hash; lookup is a binary search.

Members are keyed by the Radical hash of their path. Prototype 2 ships a
metadata table with the paths: 8-byte preamble, then per file `{u32 date,
u8 pad[4] = 00 08 00 00, u32 reserved, u32 filenameLength (includes NUL),
char filename[]}` plus 3 pad bytes (stride `0x10 + len + 3`).

Hash: `res = res * 31 + fold(c)` over the raw bytes, `fold(c) = c + 32`
for `c < 'a'`, skipping one leading backslash. Slashes are normalised to
`\` before hashing.

## Layout

- `rcf.h` — on-disk structs and offsets.
- `rcf_hash.h` — Radical hash + path normalisation.
- `rcf_file.h` / `rcf_file.cpp` — `RcfArchive` (load, list, find, extract,
  verify, pack).
- `main.cpp` — CLI.
- `utils.h` — little-endian byte helpers.
