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

#ifndef __riscv
#include <xhook.h>
#endif

namespace znn::hook::backend {

#ifndef __riscv

namespace {

struct Record {
    std::string path;
    std::string symbol;
    void* replacement = nullptr;
    void* original = nullptr;
};

std::mutex g_mutex;
std::map<std::string, Record> g_records;

std::string recordKey(const std::string& path, const char* symbol) {
    std::string key = path;
    key += '\x01';
    key += symbol;
    return key;
}

//caller library matched by regex
std::string anchorRegex(const std::string& path) {
    std::string regex = "^";
    regex.reserve(path.size() + 8);
    for (const char c : path) {
        switch (c) {
            case '.': case '\\': case '+': case '*': case '?': case '^':
            case '$': case '|': case '(': case ')': case '[': case ']':
            case '{': case '}':
                regex += '\\';
                break;
            default:
                break;
        }
        regex += c;
    }
    regex += '$';
    return regex;
}

//caller holds g_mutex
bool commitLocked() {
    xhook_clear();
    for (auto& kv : g_records) {
        Record& rec = kv.second;
        const std::string regex = anchorRegex(rec.path);
        if (xhook_register(regex.c_str(), rec.symbol.c_str(), rec.replacement, &rec.original) != 0) {
            LOGE("pltHook %s: xhook_register failed", rec.symbol.c_str());
            return false;
        }
    }
    if (xhook_refresh(0) != 0) {
        LOGE("pltHook: xhook_refresh failed");
        return false;
    }
    return true;
}

}  //namespace

bool xhookHook(const std::string& caller_path, const char* symbol, void* replacement,
               void** original) {
    if (caller_path.empty() || !symbol || !replacement) return false;

    const std::string key = recordKey(caller_path, symbol);
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_records.count(key)) {
        LOGE("pltHook %s: %s is already hooked, unhook first", symbol, caller_path.c_str());
        return false;
    }

    Record rec;
    rec.path = caller_path;
    rec.symbol = symbol;
    rec.replacement = replacement;
    rec.original = nullptr;
    g_records[key] = std::move(rec);

    if (!commitLocked() || g_records[key].original == nullptr) {
        g_records.erase(key);
        LOGE("pltHook %s: no matching relocation in %s", symbol, caller_path.c_str());
        return false;
    }
    if (original) *original = g_records[key].original;
    return true;
}

#else

bool xhookHook(const std::string& caller_path, const char* symbol, void* replacement,
               void** original) {
    (void)caller_path;
    (void)symbol;
    (void)replacement;
    (void)original;
    return false;
}

#endif

}  //namespace znn::hook::backend
