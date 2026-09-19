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

#if defined(__aarch64__) || defined(__arm__)
#include <shadowhook.h>
#endif

namespace znn::hook::backend {

#if defined(__aarch64__) || defined(__arm__)

namespace {

std::once_flag g_init_once;
int g_init_result = -1;
std::mutex g_mutex;
std::map<uintptr_t, void*> g_stubs;

}  //namespace

bool shadowhookInit() {
    std::call_once(g_init_once, [] {
        g_init_result = shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false);
    });
    if (g_init_result != 0) {
        LOGE("shadowhook_init failed: %s", shadowhook_to_errmsg(shadowhook_get_init_errno()));
        return false;
    }
    LOGI("shadowhook engine ready (mode: unique)");
    return true;
}

bool shadowhookHook(void* target, void* replacement, void** original) {
    if (!shadowhookInit()) return false;
    void* stub = shadowhook_hook_func_addr(target, replacement, original);
    if (!stub) {
        LOGE("inlineHook %p failed: %s", target, shadowhook_to_errmsg(shadowhook_get_errno()));
        return false;
    }
    std::lock_guard<std::mutex> lk(g_mutex);
    g_stubs[reinterpret_cast<uintptr_t>(target)] = stub;
    return true;
}

bool shadowhookUnhook(void* target) {
    if (!shadowhookInit()) return false;
    void* stub = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        auto it = g_stubs.find(reinterpret_cast<uintptr_t>(target));
        if (it != g_stubs.end()) {
            stub = it->second;
            g_stubs.erase(it);
        }
    }
    if (!stub || shadowhook_unhook(stub) != 0) {
        LOGE("inlineUnhook %p failed", target);
        return false;
    }
    return true;
}

#else

bool shadowhookInit() { return false; }

bool shadowhookHook(void* target, void* replacement, void** original) {
    (void)target;
    (void)replacement;
    (void)original;
    return false;
}

bool shadowhookUnhook(void* target) {
    (void)target;
    return false;
}

#endif

}  //namespace znn::hook::backend
