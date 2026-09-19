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

#ifdef __riscv
#include <rv64hook.h>
#endif

namespace znn::hook::backend {

bool rv64hookHook(void* target, void* replacement, void** original) {
#ifdef __riscv
    {
        rv64hook::ScopedRWXMemory rwx(target);
        if (rv64hook::InlineHook(target, replacement, original) == nullptr) {
            LOGE("inlineHook %p failed", target);
            return false;
        }
    }
    return true;
#else
    (void)target;
    (void)replacement;
    (void)original;
    return false;
#endif
}

bool rv64hookUnhook(void* target) {
#ifdef __riscv
    {
        rv64hook::ScopedRWXMemory rwx(target);
        if (!rv64hook::InlineUnhook(target)) return false;
    }
    return true;
#else
    (void)target;
    return false;
#endif
}

}  //namespace znn::hook::backend
