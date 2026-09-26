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

#include "hook/backend.h"

#include "log.h"

#include <map>
#include <mutex>
#include <string>

extern "C" {
#include <plti.h>
}

namespace znn::hook::backend {
namespace {

struct Record {
    std::string path;
    std::string symbol;
    void* replacement = nullptr;
    void* original = nullptr;
};

std::mutex g_mutex;
bool g_initialized = false;
struct plti g_context;
std::map<std::string, Record> g_records;

std::string recordKey(const std::string& path, const char* symbol) {
    std::string key = path;
    key += '\x01';
    key += symbol;
    return key;
}

bool ensureContextLocked() {
    if (g_initialized) return true;
    if (!plti_init(&g_context)) {
        LOGE("pltHook: plti_init failed");
        return false;
    }
    g_initialized = true;
    return true;
}

bool addLibraryLocked(const std::string& path, uintptr_t base) {
    if (plti_add_manual_lib(&g_context, path.c_str(), base)) return true;
    LOGE("pltHook: no ELF image at %s (base %p)", path.c_str(), reinterpret_cast<void*>(base));
    return false;
}

}  //namespace

bool pltiHook(const std::string& caller_path, uintptr_t caller_base, const char* symbol,
              void* replacement, void** original) {
    if (caller_path.empty() || !symbol || !replacement) return false;

    const std::string key = recordKey(caller_path, symbol);
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!ensureContextLocked()) return false;

    auto it = g_records.find(key);
    if (it != g_records.end()) {
        Record& rec = it->second;
        if (replacement != rec.original) {
            LOGE("pltHook %s: %s is already hooked, unhook first", symbol, caller_path.c_str());
            return false;
        }
        void* restore = rec.original;
        if (!plti_remove_hook(&g_context, caller_path.c_str(), symbol, &restore)) {
            LOGE("pltHook %s: plti_remove_hook failed for %s", symbol, caller_path.c_str());
            return false;
        }
        g_records.erase(it);
        if (original) *original = restore;
        return true;
    }

    if (!addLibraryLocked(caller_path, caller_base)) return false;

    void* backup = nullptr;
    if (!plti_add_hook(&g_context, caller_path.c_str(), symbol, replacement, &backup)) {
        LOGE("pltHook %s: plti_add_hook failed for %s", symbol, caller_path.c_str());
        return false;
    }
    if (!backup) {
        LOGE("pltHook %s: no matching relocation in %s", symbol, caller_path.c_str());
        return false;
    }

    Record rec;
    rec.path = caller_path;
    rec.symbol = symbol;
    rec.replacement = replacement;
    rec.original = backup;
    g_records.emplace(key, std::move(rec));
    if (original) *original = backup;
    return true;
}

}  //namespace znn::hook::backend
