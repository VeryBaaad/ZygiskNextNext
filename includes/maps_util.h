#pragma once

#include <sys/types.h>  // dev_t, ino_t

#include <cstdint>
#include <string>
#include <vector>

// /proc/<pid>/maps parsing. Kept free of any dependency on the ELF resolver so
// that both the loader and the injector can use it on its own.

namespace znn {

// A single entry of /proc/<pid>/maps.
struct MapEntry {
    uintptr_t start = 0;
    uintptr_t end = 0;
    uintptr_t offset = 0;
    dev_t dev = 0;
    ino_t inode = 0;
    uint8_t perms = 0;      // PROT_READ / PROT_WRITE / PROT_EXEC bits
    bool is_private = false;
    std::string path;
};

// Parse /proc/<pid>/maps. `pid` is "self" or a numeric pid string.
std::vector<MapEntry> parseMaps(const std::string& pid);

// Parse a maps file directly (used by parseMaps(); also exposed for testing).
std::vector<MapEntry> parseMapsPath(const std::string& path);

// Scan a parsed maps list for the first mapping of a library matching `name`
// (full path if it contains '/', otherwise basename). Returns the load base
// (start of the offset-0 mapping), or 0 if not found. `whole_size`, when
// non-zero, is the file size of the library: mappings that span the whole
// file (page-rounded) are skipped, because they are raw whole-file data views
// (e.g. created by an ElfImage's own mmap) rather than the loaded image.
uintptr_t findLibraryBaseInMaps(const std::vector<MapEntry>& maps, const char* name,
                                size_t whole_size = 0);

// Scan /proc/self/maps for the first mapping of a library matching `name`
// (full path if it contains '/', otherwise basename). Returns the load base,
// or 0 if not found. See findLibraryBaseInMaps() for the `whole_size` skip.
uintptr_t findLibraryBase(const char* name, size_t whole_size = 0);

}  // namespace znn
