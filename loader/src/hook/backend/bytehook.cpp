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
#include <bytehook.h>
#endif

namespace znn::hook::backend {

#ifndef __riscv

namespace {

std::once_flag g_init_once;
int g_init_result = -1;
std::mutex g_mutex;
std::map<std::string, void*> g_stubs;
std::map<std::string, void*> g_originals;

std::string hookKey(const std::string& caller_path, const char* symbol) {
    std::string key = caller_path;
    key += '\x01';
    key += symbol;
    return key;
}

}  //namespace

extern "C" {
static void bytehookHooked(bytehook_stub_t stub, int status_code, const char* caller_path_name,
                           const char* sym_name, void* new_func, void* prev_func, void* arg) {
    (void)stub;
    (void)caller_path_name;
    (void)sym_name;
    (void)new_func;
    if (status_code == 0 && prev_func) {
        auto* out = static_cast<void**>(arg);
        if (out && *out == nullptr) *out = prev_func;
    }
}
}

bool bytehookInit() {
    std::call_once(g_init_once, [] {
#if defined(__aarch64__) || defined(__arm__)
        shadowhookInit();
#endif
        g_init_result = bytehook_init(BYTEHOOK_MODE_MANUAL, false);
    });
    if (g_init_result != 0) {
        LOGE("bytehook_init failed: %d", g_init_result);
        return false;
    }
    LOGI("bytehook engine ready (mode: manual)");
    return true;
}

bool bytehookHook(const std::string& caller_path, const char* symbol, void* replacement,
                  void** original) {
    if (!bytehookInit()) return false;

    const std::string key = hookKey(caller_path, symbol);
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        auto stub_it = g_stubs.find(key);
        if (stub_it != g_stubs.end()) {
            //unhook by passing original
            if (replacement == g_originals[key]) {
                if (bytehook_unhook(stub_it->second) != 0) {
                    LOGE("pltHook %s: bytehook_unhook failed", symbol);
                    return false;
                }
                if (original) *original = g_originals[key];
                g_originals.erase(key);
                g_stubs.erase(stub_it);
                return true;
            }
            LOGE("pltHook %s: %s is already hooked, unhook first", symbol, caller_path.c_str());
            return false;
        }
    }

    void* backup = nullptr;
    const bytehook_stub_t stub =
        bytehook_hook_single(caller_path.c_str(), nullptr, symbol, replacement, bytehookHooked,
                             &backup);
    if (!stub) {
        LOGE("pltHook %s: bytehook_hook_single failed for %s", symbol, caller_path.c_str());
        return false;
    }
    if (!backup) {
        LOGE("pltHook %s: no matching relocation in %s", symbol, caller_path.c_str());
        bytehook_unhook(stub);
        return false;
    }
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        g_stubs[key] = stub;
        g_originals[key] = backup;
    }
    if (original) *original = backup;
    return true;
}

#else

bool bytehookInit() { return false; }

bool bytehookHook(const std::string& caller_path, const char* symbol, void* replacement,
                  void** original) {
    (void)caller_path;
    (void)symbol;
    (void)replacement;
    (void)original;
    return false;
}

#endif

}  //namespace znn::hook::backend
