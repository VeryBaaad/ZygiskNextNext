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

#include "api/api.h"

#include "hook/inline.h"
#include "hook/plt.h"
#include "log.h"
#include "utils/elf_util.h"

#include <algorithm>
#include <mutex>
#include <set>
#include <vector>

namespace znn::api {
namespace {

std::mutex g_hook_mutex;
std::set<uintptr_t> g_hooked;

void logUnmappedTarget(uintptr_t target, const std::vector<MapEntry>& maps) {
    LOGW("inlineHook target %p is not mapped", reinterpret_cast<void*>(target));
    for (const auto& m : maps) {
        if (m.path.find("libc.so") == std::string::npos) continue;
        LOGI("  map %p-%p off=0x%lx %s", reinterpret_cast<void*>(m.start),
             reinterpret_cast<void*>(m.end), static_cast<unsigned long>(m.offset), m.path.c_str());
    }
}

}  //namespace

int pltHook(void* base, const char* symbol, void* hook, void** original) {
    if (!base || !symbol || !hook) return ZN_FAILED;
    return znn::hook::pltHook(base, symbol, hook, original) ? ZN_SUCCESS : ZN_FAILED;
}

int inlineHook(void* target, void* addr, void** original) {
    if (!target || !addr) return ZN_FAILED;

    const uintptr_t t = reinterpret_cast<uintptr_t>(target);
    const auto maps = znn::parseMaps("self");
    const auto mapped = std::find_if(maps.begin(), maps.end(), [t](const MapEntry& m) {
        return t >= m.start && t < m.end;
    });
    if (mapped != maps.end()) {
        LOGI("inlineHook target %p -> %s+0x%lx", reinterpret_cast<void*>(t), mapped->path.c_str(),
             static_cast<unsigned long>(t - mapped->start));
    } else {
        logUnmappedTarget(t, maps);
    }

    //claim the address before hooking it: two modules racing for the same address
    //must not both be told they own it
    {
        std::lock_guard<std::mutex> lk(g_hook_mutex);
        if (!g_hooked.insert(t).second) return ZN_FAILED;
    }
    if (!znn::hook::inlineHook(target, addr, original)) {
        std::lock_guard<std::mutex> lk(g_hook_mutex);
        g_hooked.erase(t);
        return ZN_FAILED;
    }
    return ZN_SUCCESS;
}

int inlineUnhook(void* target) {
    if (!target) return ZN_FAILED;

    const uintptr_t t = reinterpret_cast<uintptr_t>(target);
    if (!znn::hook::inlineUnhook(target)) return ZN_FAILED;

    std::lock_guard<std::mutex> lk(g_hook_mutex);
    g_hooked.erase(t);
    return ZN_SUCCESS;
}

}  //namespace znn::api
