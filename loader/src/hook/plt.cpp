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

#include "hook/plt.h"

#include "hook/backend.h"
#include "hook/engine.h"
#include "log.h"
#include "utils/elf_util.h"

#include <vector>

namespace znn::hook {
namespace {

const MapEntry* findMapping(const std::vector<MapEntry>& maps, uintptr_t base) {
    for (const auto& m : maps) {
        if (m.inode == 0) continue;
        if (base >= m.start && base < m.end) return &m;
        if (m.start >= m.offset && base == m.start - m.offset) return &m;
    }
    return nullptr;
}

const MapEntry* firstMappingOfFile(const std::vector<MapEntry>& maps, const MapEntry& hit) {
    for (const auto& m : maps) {
        if (m.dev == hit.dev && m.inode == hit.inode && m.offset == 0) return &m;
    }
    return &hit;
}

}  //namespace

bool pltHook(void* base, const char* symbol, void* replacement, void** original) {
    if (!base || !symbol || !replacement) return false;
    ensurePltEngineReady();

    const uintptr_t b = reinterpret_cast<uintptr_t>(base);
    const auto maps = parseMaps("self");
    const MapEntry* hit = findMapping(maps, b);
    if (!hit) {
        LOGE("pltHook %s: base %p not found in maps", symbol, base);
        return false;
    }
    const MapEntry* entry = firstMappingOfFile(maps, *hit);
    if (entry->path.empty()) {
        LOGE("pltHook %s: base %p has no path in maps", symbol, base);
        return false;
    }
    LOGI("pltHook base=%p symbol=%s path=%s dev=%llu inode=%llu", base, symbol, entry->path.c_str(),
         static_cast<unsigned long long>(entry->dev),
         static_cast<unsigned long long>(entry->inode));

    switch (pltEngine()) {
        case PltEngine::kLsplt:
            return backend::lspltHook(entry->dev, entry->inode, symbol, replacement, original);
        case PltEngine::kByteHook:
            return backend::bytehookHook(entry->path, symbol, replacement, original);
        case PltEngine::kXHook:
            return backend::xhookHook(entry->path, symbol, replacement, original);
    }
    return false;
}

}  //namespace znn::hook
