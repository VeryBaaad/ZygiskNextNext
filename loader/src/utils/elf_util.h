/*
 * This file is part of Zygisk Next Next.
 *
 * Zygisk Next Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zygisk Next Next is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zygisk Next Next. If not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2026 VeryBaaad <verybaaad@outlook.com>
 */

#pragma once

#include <elf.h>
#include <link.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace znn {

struct SymbolInfo {
    std::string name;
    uintptr_t addr;
    size_t size;
};

class ElfImage {
public:
    ElfImage(std::string path, uintptr_t base = 0);
    ~ElfImage();

    ElfImage(const ElfImage&) = delete;
    ElfImage& operator=(const ElfImage&) = delete;

    bool valid() const { return valid_; }
    uintptr_t base() const { return base_; }
    const std::string& path() const { return path_; }

    const SymbolInfo* lookup(const char* name, bool prefix) const;

    uintptr_t runtimeLookup(const char* name, size_t* size = nullptr) const;

    void forEach(const std::function<bool(const char*, uintptr_t, size_t)>& cb) const;

private:
    void ensureParsed() const;
    void parseSymbols(const ElfW(Shdr)* str_sh, const char* strtab, const ElfW(Sym)* symtab,
                      size_t count, uintptr_t bias) const;
    bool parseGnuDebugData(const uint8_t* data, size_t size) const;

    std::string path_;
    uintptr_t base_ = 0;
    bool valid_ = false;
    bool is_dyn_ = false;

    uint8_t* file_ = nullptr;
    size_t file_size_ = 0;
    const ElfW(Ehdr)* ehdr_ = nullptr;
    const char* section_names_ = nullptr;

    mutable std::vector<uint8_t> debugdata_;
    mutable const ElfW(Ehdr)* debugdata_ehdr_ = nullptr;

    mutable bool parsed_ = false;
    mutable std::vector<SymbolInfo> symbols_;
};

struct MapEntry {
    uintptr_t start = 0;
    uintptr_t end = 0;
    uintptr_t offset = 0;
    dev_t dev = 0;
    ino_t inode = 0;
    uint8_t perms = 0;
    bool is_private = false;
    std::string path;
};

std::vector<MapEntry> parseMaps(const std::string& pid);

std::vector<MapEntry> parseMapsPath(const std::string& path);

uintptr_t findLibraryBaseInMaps(const std::vector<MapEntry>& maps, const char* name,
                                size_t whole_size = 0);

uintptr_t findLibraryBase(const char* name, size_t whole_size = 0);

uintptr_t resolveLibrarySymbol(const char* lib_name, const char* symbol, size_t* size = nullptr);

}  //namespace znn
