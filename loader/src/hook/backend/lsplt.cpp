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

namespace znn::hook::backend {

bool lspltHook(dev_t dev, ino_t inode, const char* symbol, void* replacement, void** original) {
    void* backup = nullptr;
    if (!lsplt::RegisterHook(dev, inode, symbol, replacement, &backup)) {
        LOGE("pltHook %s: RegisterHook failed", symbol);
        return false;
    }
    if (!lsplt::CommitHook()) {
        LOGE("pltHook %s: CommitHook failed", symbol);
        return false;
    }
    if (!backup) {
        LOGE("pltHook %s: symbol not found in PLT", symbol);
        return false;
    }
    if (original) *original = backup;
    return true;
}

}  //namespace znn::hook::backend
