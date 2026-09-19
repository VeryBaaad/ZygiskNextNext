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

#include "elf_util.h"

#include <android/log.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <unordered_set>

#include "LzmaDec.h"

#define ELF_LOGW(...) __android_log_print(ANDROID_LOG_WARN, "ZNNloader", __VA_ARGS__)

namespace znn {

//LZMA for .gnu_debugdata

static void* LzmaAlloc(ISzAllocPtr, size_t size) { return malloc(size); }
static void LzmaFree(ISzAllocPtr, void* addr) { free(addr); }
static const ISzAlloc g_lzma_alloc = {LzmaAlloc, LzmaFree};

//[props][dict][size][data]
static bool lzmaAloneDecompress(const uint8_t* in, size_t in_size, std::vector<uint8_t>& out) {
    if (in_size < 13) return false;

    //props: lc/lp/pb + dict size
    const Byte* props = in;

    uint64_t out_size = 0;
    for (int i = 0; i < 8; ++i) out_size |= static_cast<uint64_t>(in[5 + i]) << (8 * i);

    if (out_size == 0 || out_size == UINT64_MAX) return false;

    const uint8_t* data = in + 13;
    size_t data_size = in_size - 13;

    out.resize(static_cast<size_t>(out_size));
    SizeT dest_len = static_cast<SizeT>(out_size);
    SizeT src_len = static_cast<SizeT>(data_size);
    ELzmaStatus status;

    SRes res = LzmaDecode(out.data(), &dest_len, data, &src_len, props, 5, LZMA_FINISH_END, &status,
                          &g_lzma_alloc);
    if (res != SZ_OK) return false;

    out.resize(dest_len);
    return true;
}

static bool isElf(const uint8_t* p, size_t size) {
    return size >= SELFMAG + 1 && p[EI_MAG0] == ELFMAG0 && p[EI_MAG1] == ELFMAG1 &&
           p[EI_MAG2] == ELFMAG2 && p[EI_MAG3] == ELFMAG3;
}

static bool sectionInFile(const ElfW(Shdr)& sh, size_t file_size) {
    return sh.sh_offset <= file_size && sh.sh_size <= file_size - sh.sh_offset;
}

static bool sectionTableInFile(const ElfW(Ehdr)* ehdr, size_t file_size) {
    const size_t table_size = static_cast<size_t>(ehdr->e_shnum) * sizeof(ElfW(Shdr));
    const size_t table_offset = static_cast<size_t>(ehdr->e_shoff);
    if (table_offset > file_size) return false;
    return table_size <= file_size - table_offset;
}

static bool mapMatchesName(const MapEntry& m, const char* name) {
    if (m.path.empty() || m.path[0] == '[') return false;
    if (strchr(name, '/') != nullptr) return m.path == name;

    const char* slash = strrchr(m.path.c_str(), '/');
    return strcmp(slash ? slash + 1 : m.path.c_str(), name) == 0;
}

//soname or path, via maps
std::string resolveMapPath(const char* name) {
    for (const auto& m : parseMaps("self")) {
        if (mapMatchesName(m, name)) return m.path;
    }
    return {};
}

ElfImage::ElfImage(std::string path, uintptr_t base) : path_(std::move(path)), base_(base) {
    int fd = open(path_.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        std::string resolved = resolveMapPath(path_.c_str());
        if (resolved.empty()) return;
        path_ = std::move(resolved);
        fd = open(path_.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd < 0) return;
    }

    struct stat st {};
    if (fstat(fd, &st) != 0 || st.st_size < static_cast<off_t>(sizeof(ElfW(Ehdr)))) {
        close(fd);
        return;
    }
    file_size_ = static_cast<size_t>(st.st_size);

    if (base_ == 0) base_ = findLibraryBase(path_.c_str(), file_size_);

    file_ = static_cast<uint8_t*>(mmap(nullptr, file_size_, PROT_READ, MAP_PRIVATE, fd, 0));
    close(fd);
    if (file_ == MAP_FAILED) {
        file_ = nullptr;
        return;
    }

    ehdr_ = reinterpret_cast<const ElfW(Ehdr)*>(file_);
    if (!isElf(file_, file_size_) || !sectionTableInFile(ehdr_, file_size_)) return;

    is_dyn_ = (ehdr_->e_type == ET_DYN);
    if (!is_dyn_) {
        //ET_EXEC is loaded at its link-time addresses: there is no bias to add
        base_ = 0;
    } else if (base_ == 0) {
        return;
    } else if (memcmp(reinterpret_cast<const void*>(base_), ELFMAG, SELFMAG) != 0) {
        ELF_LOGW("ElfImage %s: base %p does not look like an ELF header", path_.c_str(),
                 reinterpret_cast<void*>(base_));
        return;
    }

    if (ehdr_->e_shstrndx != SHN_UNDEF && ehdr_->e_shstrndx < ehdr_->e_shnum) {
        const ElfW(Shdr)* shdrs = reinterpret_cast<const ElfW(Shdr)*>(file_ + ehdr_->e_shoff);
        const ElfW(Shdr)* shstr = &shdrs[ehdr_->e_shstrndx];
        if (sectionInFile(*shstr, file_size_))
            section_names_ = reinterpret_cast<const char*>(file_ + shstr->sh_offset);
    }

    valid_ = true;
}

ElfImage::~ElfImage() {
    if (file_) munmap(file_, file_size_);
}

void ElfImage::parseSymbols(const ElfW(Shdr)* str_sh, const char* strtab,
                            const ElfW(Sym)* symtab, size_t count, uintptr_t bias) const {
    if (!strtab || !symtab) return;
    for (size_t i = 0; i < count; ++i) {
        const ElfW(Sym)& s = symtab[i];
        if (s.st_shndx == SHN_UNDEF) continue;
        if (s.st_name == 0) continue;
        if (s.st_name >= str_sh->sh_size) continue;
        const char* name = strtab + s.st_name;
        if (!name[0]) continue;

        SymbolInfo info;
        info.name = name;
        info.addr = bias + s.st_value;
        info.size = s.st_size;
        symbols_.push_back(std::move(info));
    }
}

bool ElfImage::parseGnuDebugData(const uint8_t* data, size_t size) const {
    for (size_t off : {static_cast<size_t>(4), static_cast<size_t>(0)}) {
        if (size <= off + 13) continue;
        std::vector<uint8_t> out;
        if (!lzmaAloneDecompress(data + off, size - off, out)) continue;
        if (!isElf(out.data(), out.size())) continue;

        debugdata_ = std::move(out);
        debugdata_ehdr_ = reinterpret_cast<const ElfW(Ehdr)*>(debugdata_.data());
        return true;
    }
    return false;
}

void ElfImage::ensureParsed() const {
    if (parsed_) return;
    parsed_ = true;

    const uintptr_t bias = is_dyn_ ? base_ : 0;
    const ElfW(Shdr)* shdrs = reinterpret_cast<const ElfW(Shdr)*>(file_ + ehdr_->e_shoff);
    const size_t shnum = ehdr_->e_shnum;
    const size_t shstr_size =
        (ehdr_->e_shstrndx != SHN_UNDEF && ehdr_->e_shstrndx < shnum) ? shdrs[ehdr_->e_shstrndx].sh_size : 0;

    auto sec_name = [&](const ElfW(Shdr)& sh) -> const char* {
        if (!section_names_ || sh.sh_name >= shstr_size) return "";
        return section_names_ + sh.sh_name;
    };

    //walk section headers
    for (size_t i = 0; i < shnum; ++i) {
        const ElfW(Shdr)& sh = shdrs[i];

        if (sh.sh_type == SHT_SYMTAB || sh.sh_type == SHT_DYNSYM) {
            if (sh.sh_link >= shnum) continue;
            const ElfW(Shdr)& str_sh = shdrs[sh.sh_link];
            if (!sectionInFile(sh, file_size_) || !sectionInFile(str_sh, file_size_)) continue;
            const char* strtab = reinterpret_cast<const char*>(file_ + str_sh.sh_offset);
            const ElfW(Sym)* symtab = reinterpret_cast<const ElfW(Sym)*>(file_ + sh.sh_offset);
            size_t count = (sh.sh_entsize) ? sh.sh_size / sh.sh_entsize : 0;
            parseSymbols(&str_sh, strtab, symtab, count, bias);
        } else if (sh.sh_type == SHT_PROGBITS && strcmp(sec_name(sh), ".gnu_debugdata") == 0 &&
                   sectionInFile(sh, file_size_)) {
            parseGnuDebugData(file_ + sh.sh_offset, sh.sh_size);
        }
    }

    if (debugdata_ehdr_ && sectionTableInFile(debugdata_ehdr_, debugdata_.size())) {
        const ElfW(Ehdr)* deh = debugdata_ehdr_;
        const ElfW(Shdr)* dshdrs =
            reinterpret_cast<const ElfW(Shdr)*>(debugdata_.data() + deh->e_shoff);
        const size_t dshnum = deh->e_shnum;
        for (size_t i = 0; i < dshnum; ++i) {
            const ElfW(Shdr)& sh = dshdrs[i];
            if (sh.sh_type != SHT_SYMTAB) continue;
            if (sh.sh_link >= dshnum) continue;
            const ElfW(Shdr)& str_sh = dshdrs[sh.sh_link];
            if (!sectionInFile(sh, debugdata_.size()) || !sectionInFile(str_sh, debugdata_.size())) {
                continue;
            }
            const char* strtab = reinterpret_cast<const char*>(debugdata_.data() + str_sh.sh_offset);
            const ElfW(Sym)* symtab = reinterpret_cast<const ElfW(Sym)*>(debugdata_.data() + sh.sh_offset);
            size_t count = (sh.sh_entsize) ? sh.sh_size / sh.sh_entsize : 0;
            parseSymbols(&str_sh, strtab, symtab, count, bias);
        }
    }

    std::unordered_set<std::string> seen;
    std::vector<SymbolInfo> unique;
    unique.reserve(symbols_.size());
    for (auto& s : symbols_) {
        std::string key = s.name;
        key += '\0';
        key += std::to_string(s.addr);
        if (seen.insert(std::move(key)).second) unique.push_back(std::move(s));
    }
    symbols_ = std::move(unique);
}

const SymbolInfo* ElfImage::lookup(const char* name, bool prefix) const {
    ensureParsed();
    if (prefix) {
        size_t n = strlen(name);
        for (const auto& s : symbols_) {
            if (s.name.compare(0, n, name) == 0) return &s;
        }
    } else {
        for (const auto& s : symbols_) {
            if (s.name == name) return &s;
        }
    }
    return nullptr;
}

void ElfImage::forEach(const std::function<bool(const char*, uintptr_t, size_t)>& cb) const {
    ensureParsed();
    for (const auto& s : symbols_) {
        if (!cb(s.name.c_str(), s.addr, s.size)) break;
    }
}

uintptr_t ElfImage::runtimeLookup(const char* name, size_t* size) const {
    if (!base_ || !name) return 0;

    const ElfW(Ehdr)* eh = reinterpret_cast<const ElfW(Ehdr)*>(base_);
    if (memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0) return 0;
#if defined(__LP64__)
    if (eh->e_ident[EI_CLASS] != ELFCLASS64) return 0;
#else
    if (eh->e_ident[EI_CLASS] != ELFCLASS32) return 0;
#endif
    if (eh->e_ident[EI_DATA] != ELFDATA2LSB) return 0;

    const uint8_t* img = reinterpret_cast<const uint8_t*>(base_);
    const ElfW(Phdr)* ph = reinterpret_cast<const ElfW(Phdr)*>(img + eh->e_phoff);
    const ElfW(Dyn)* dyn = nullptr;
    for (int i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type == PT_DYNAMIC) {
            dyn = reinterpret_cast<const ElfW(Dyn)*>(img + ph[i].p_vaddr);
            break;
        }
    }
    if (!dyn) return 0;

    const ElfW(Sym)* symtab = nullptr;
    const char* strtab = nullptr;
    size_t strsz = 0;
    for (const ElfW(Dyn)* d = dyn; d->d_tag != DT_NULL; ++d) {
        switch (d->d_tag) {
            case DT_SYMTAB:
                symtab = reinterpret_cast<const ElfW(Sym)*>(img + d->d_un.d_ptr);
                break;
            case DT_STRTAB:
                strtab = reinterpret_cast<const char*>(img + d->d_un.d_ptr);
                break;
            case DT_STRSZ:
                strsz = static_cast<size_t>(d->d_un.d_val);
                break;
            default:
                break;
        }
    }
    if (!symtab || !strtab || strsz == 0) return 0;

    const char* sym_end = reinterpret_cast<const char*>(symtab);
    size_t max_syms = strtab > sym_end ? static_cast<size_t>(strtab - sym_end) / sizeof(ElfW(Sym))
                                       : 0;
    if (max_syms == 0 || max_syms > (1u << 20)) max_syms = 1u << 20;

    for (size_t i = 1; i < max_syms; ++i) {
        const ElfW(Sym)& s = symtab[i];
        if (s.st_name == 0 || s.st_name >= strsz) continue;
        if (strcmp(strtab + s.st_name, name) == 0) {
            if (size) *size = s.st_size;
            return base_ + s.st_value;
        }
    }
    return 0;
}

std::vector<MapEntry> parseMaps(const std::string& pid) {
    return parseMapsPath("/proc/" + pid + "/maps");
}

std::vector<MapEntry> parseMapsPath(const std::string& path) {
    std::vector<MapEntry> result;
    FILE* f = fopen(path.c_str(), "re");
    if (!f) return result;

    char* line = nullptr;
    size_t cap = 0;
    while (getline(&line, &cap, f) > 0) {
        uintptr_t start = 0, end = 0, offset = 0;
        unsigned int devmaj = 0, devmin = 0;
        unsigned long inode = 0;
        char perms[8] = {0};
        char mpath[512] = {0};
        int n = sscanf(line, "%" SCNxPTR "-%" SCNxPTR " %7s %" SCNxPTR " %x:%x %lu %511[^\n]", &start,
                       &end, perms, &offset, &devmaj, &devmin, &inode, mpath);
        if (n < 7) continue;

        MapEntry e;
        e.start = start;
        e.end = end;
        e.offset = offset;
        e.dev = static_cast<dev_t>(makedev(devmaj, devmin));
        e.inode = static_cast<ino_t>(inode);
        e.is_private = (perms[3] == 'p');
        if (perms[0] == 'r') e.perms |= PROT_READ;
        if (perms[1] == 'w') e.perms |= PROT_WRITE;
        if (perms[2] == 'x') e.perms |= PROT_EXEC;
        e.path = mpath;
        result.push_back(std::move(e));
    }
    free(line);
    fclose(f);
    return result;
}

uintptr_t findLibraryBaseInMaps(const std::vector<MapEntry>& maps, const char* name,
                                size_t whole_size) {
    for (const auto& m : maps) {
        if (!mapMatchesName(m, name)) continue;

        if (whole_size != 0) {
            const uintptr_t span = m.end - m.start;
            if (span >= whole_size && span - whole_size < 4096) continue;
        }

        if (m.start >= m.offset) return m.start - m.offset;
    }
    return 0;
}

uintptr_t findLibraryBase(const char* name, size_t whole_size) {
    return findLibraryBaseInMaps(parseMaps("self"), name, whole_size);
}

uintptr_t resolveLibrarySymbol(const char* lib_name, const char* symbol, size_t* size) {
    ElfImage img(lib_name, 0);
    if (!img.valid()) return 0;
    const SymbolInfo* s = img.lookup(symbol, false);
    if (!s) return 0;
    if (size) *size = s->size;
    return s->addr;
}

}  //namespace znn
