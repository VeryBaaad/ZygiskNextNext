#pragma once

#include <elf.h>
#include <link.h>  // ElfW()

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// A self contained ELF symbol resolver. It parses .dynsym / .symtab and, when
// present, the compressed .gnu_debugdata ("mini debuginfo") section so that
// local symbols of stripped libraries can still be resolved.
//
// This is used by the ZN loader to implement the ZygiskNextAPI symbol
// resolver. The /proc/<pid>/maps helpers it needs to locate a load base live in
// maps_util.h.

namespace znn {

struct SymbolInfo {
    std::string name;
    uintptr_t addr;  // runtime (virtual) address
    size_t size;
};

class ElfImage {
public:
    // `path`: absolute path or file name of the library.
    // `base`: runtime load base; pass 0 to auto detect via /proc/self/maps.
    ElfImage(std::string path, uintptr_t base = 0);
    ~ElfImage();

    ElfImage(const ElfImage&) = delete;
    ElfImage& operator=(const ElfImage&) = delete;

    bool valid() const { return valid_; }
    uintptr_t base() const { return base_; }
    const std::string& path() const { return path_; }

    // Lookup by exact name, or by prefix when `prefix` is true.
    // Returns nullptr when not found. `size` (optional) receives st_size.
    const SymbolInfo* lookup(const char* name, bool prefix) const;

    // Resolve a symbol against the library's *runtime* in-memory image
    // (dynsym of the mapped ELF). Unlike dlsym(), this also finds symbols
    // that are not exported (hidden visibility), and it is authoritative for
    // what is actually mapped. Returns 0 when not found or when `base_` is
    // not a valid load bias.
    uintptr_t runtimeLookup(const char* name, size_t* size = nullptr) const;

    // Walk every symbol. Returning false from `cb` stops the walk.
    void forEach(const std::function<bool(const char*, uintptr_t, size_t)>& cb) const;

private:
    void ensureParsed() const;
    void parseSymbols(const ElfW(Shdr)* str_sh, const char* strtab, const ElfW(Sym)* symtab,
                      size_t count, uintptr_t bias) const;
    bool parseGnuDebugData(const uint8_t* data, size_t size) const;

    std::string path_;
    uintptr_t base_ = 0;
    bool valid_ = false;
    bool is_dyn_ = false;  // ET_DYN => symbol values are relative to base_

    uint8_t* file_ = nullptr;  // mmap of the library file
    size_t file_size_ = 0;
    const ElfW(Ehdr)* ehdr_ = nullptr;
    const char* section_names_ = nullptr;

    // Decompressed .gnu_debugdata mini ELF (owned memory).
    mutable std::vector<uint8_t> debugdata_;
    mutable const ElfW(Ehdr)* debugdata_ehdr_ = nullptr;

    mutable bool parsed_ = false;
    mutable std::vector<SymbolInfo> symbols_;
};

// Resolve the runtime address of `symbol` inside `lib_name`.
uintptr_t resolveLibrarySymbol(const char* lib_name, const char* symbol, size_t* size = nullptr);

}  // namespace znn
