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

#include "maps_util.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <sys/types.h>

#include <utility>
#include <vector>

namespace znn {

// ---------------------------------------------------------------------------
// Maps helpers
// ---------------------------------------------------------------------------

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
        // 7f000000-7f001000 r--p 00000000 fe:01 123 /path/to/lib.so
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
    bool want_basename = (strchr(name, '/') == nullptr);
    for (const auto& m : maps) {
        if (m.path.empty() || m.path[0] == '[') continue;

        const char* basename = strrchr(m.path.c_str(), '/');
        basename = basename ? basename + 1 : m.path.c_str();

        bool match = want_basename ? (strcmp(basename, name) == 0) : (m.path == name);
        if (!match) continue;

        // Skip a raw whole-file mapping of the same library (as created by
        // ElfImage's own mmap, or by another still-live resolver). It is a
        // plain data view of the file, not the image loaded by the linker,
        // and would otherwise be picked up as the load base when it sorts
        // below the real mapping.
        if (whole_size != 0) {
            const uintptr_t span = m.end - m.start;
            if (span >= whole_size && span - whole_size < 4096) continue;
        }

        // For an ET_DYN image every map entry's file offset is relative to the
        // load bias, so start - offset yields the bias from ANY segment — do
        // not rely on the offset-0 header map being present or listed first.
        if (m.start >= m.offset) return m.start - m.offset;
    }
    return 0;
}

uintptr_t findLibraryBase(const char* name, size_t whole_size) {
    return findLibraryBaseInMaps(parseMaps("self"), name, whole_size);
}

}  // namespace znn
