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

#include <lsplt.hpp>

#include <map>
#include <mutex>
#include <string>

namespace znn::hook::backend {
namespace {

std::mutex g_mutex;
std::map<std::string, void*> g_backups;

void** backupSlot(dev_t dev, ino_t inode, const char* symbol) {
    std::string key = std::to_string(static_cast<unsigned long long>(dev));
    key += ':';
    key += std::to_string(static_cast<unsigned long long>(inode));
    key += '\x01';
    key += symbol;

    std::lock_guard<std::mutex> lk(g_mutex);
    return &g_backups[key];
}

}  //namespace

bool lspltHook(dev_t dev, ino_t inode, const char* symbol, void* replacement, void** original) {
    void** const slot = backupSlot(dev, inode, symbol);
    *slot = nullptr;

    if (!lsplt::RegisterHook(dev, inode, symbol, replacement, slot)) {
        LOGE("pltHook %s: RegisterHook failed", symbol);
        return false;
    }

    const bool committed = lsplt::CommitHook();
    if (*slot) {
        if (!committed) {
            LOGW("pltHook %s: the commit failed after the hook was installed", symbol);
        }
        if (original) *original = *slot;
        return true;
    }
    if (!committed) {
        LOGE("pltHook %s: CommitHook failed", symbol);
        return false;
    }
    LOGE("pltHook %s: symbol not found in PLT", symbol);
    return false;
}

}  //namespace znn::hook::backend
