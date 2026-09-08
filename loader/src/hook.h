#pragma once

#include "utils/elf_util.h"

#include <android/log.h>

#include <sys/types.h>

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <string.h>
#include <unistd.h>
#include <vector>

#ifndef __riscv
#include <dobby.h>
#endif

#ifdef __riscv
#include <rv64hook.h>
#endif

#if defined(__aarch64__) || defined(__arm__)
#include <shadowhook.h>
#endif

#ifndef __riscv
#include <bytehook.h>
#endif

#include <lsplt.hpp>

namespace znn::hook {

enum class InlineEngine { kDobby, kShadowHook, kRv64Hook };
enum class PltEngine { kLsplt, kByteHook };

inline const char* inlineEngineName(InlineEngine e) {
    switch (e) {
        case InlineEngine::kDobby: return "dobby";
        case InlineEngine::kShadowHook: return "shadowhook";
        case InlineEngine::kRv64Hook: return "rv64hook";
    }
    return "unknown";
}

inline const char* pltEngineName(PltEngine e) {
    switch (e) {
        case PltEngine::kLsplt: return "lsplt";
        case PltEngine::kByteHook: return "bytehook";
    }
    return "unknown";
}

inline bool inlineEngineSupported(InlineEngine e) {
    switch (e) {
        case InlineEngine::kDobby:
#ifndef __riscv
            return true;
#else
            return false;
#endif
        case InlineEngine::kShadowHook:
#if defined(__aarch64__) || defined(__arm__)
            return true;
#else
            return false;
#endif
        case InlineEngine::kRv64Hook:
#ifdef __riscv
            return true;
#else
            return false;
#endif
    }
    return false;
}

inline bool pltEngineSupported(PltEngine e) {
    switch (e) {
        case PltEngine::kLsplt:
            return true;
        case PltEngine::kByteHook:
#ifndef __riscv
            return true;
#else
            return false;
#endif
    }
    return false;
}

inline InlineEngine defaultInlineEngine() {
#ifdef __riscv
    return InlineEngine::kRv64Hook;
#else
    return InlineEngine::kDobby;
#endif
}

inline PltEngine defaultPltEngine() { return PltEngine::kLsplt; }

inline bool parseInlineEngine(const char* name, InlineEngine* out) {
    if (!name) return false;
    InlineEngine e;
    if (strcmp(name, "dobby") == 0) {
        e = InlineEngine::kDobby;
    } else if (strcmp(name, "shadowhook") == 0) {
        e = InlineEngine::kShadowHook;
    } else if (strcmp(name, "rv64hook") == 0) {
        e = InlineEngine::kRv64Hook;
    } else {
        return false;
    }
    if (!inlineEngineSupported(e)) return false;
    *out = e;
    return true;
}

inline bool parsePltEngine(const char* name, PltEngine* out) {
    if (!name) return false;
    PltEngine e;
    if (strcmp(name, "lsplt") == 0) {
        e = PltEngine::kLsplt;
    } else if (strcmp(name, "bytehook") == 0) {
        e = PltEngine::kByteHook;
    } else {
        return false;
    }
    if (!pltEngineSupported(e)) return false;
    *out = e;
    return true;
}

inline InlineEngine g_inline_engine = defaultInlineEngine();
inline PltEngine g_plt_engine = defaultPltEngine();

inline void setInlineEngine(InlineEngine e) {
    if (inlineEngineSupported(e)) g_inline_engine = e;
}

inline void setPltEngine(PltEngine e) {
    if (pltEngineSupported(e)) g_plt_engine = e;
}

#if defined(__aarch64__) || defined(__arm__)
inline std::once_flag g_shadowhook_init_once;
inline int g_shadowhook_init_result = -1;
inline std::mutex g_shadowhook_mutex;
inline std::map<uintptr_t, void*> g_shadowhook_stubs;

inline bool shadowhookInit() {
    std::call_once(g_shadowhook_init_once, [] {
        g_shadowhook_init_result = shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false);
    });
    if (g_shadowhook_init_result != 0) {
        __android_log_print(ANDROID_LOG_ERROR, "ZNNloader", "shadowhook_init failed: %s",
                            shadowhook_to_errmsg(shadowhook_get_init_errno()));
        return false;
    }
    __android_log_print(ANDROID_LOG_INFO, "ZNNloader", "shadowhook engine ready (mode: unique)");
    return true;
}
#endif

#ifndef __riscv
inline std::once_flag g_bytehook_init_once;
inline int g_bytehook_init_result = -1;
inline std::mutex g_bytehook_mutex;
inline std::map<std::string, void*> g_bytehook_stubs;
inline std::map<std::string, void*> g_bytehook_originals;
#endif

inline bool bytehookInit() {
#ifndef __riscv
    std::call_once(g_bytehook_init_once, [] {
#if defined(__aarch64__) || defined(__arm__)
        (void)shadowhookInit();
#endif
        g_bytehook_init_result = bytehook_init(BYTEHOOK_MODE_MANUAL, false);
    });
    if (g_bytehook_init_result != 0) {
        __android_log_print(ANDROID_LOG_ERROR, "ZNNloader", "bytehook_init failed: %d",
                            g_bytehook_init_result);
        return false;
    }
    __android_log_print(ANDROID_LOG_INFO, "ZNNloader", "bytehook engine ready (mode: manual)");
    return true;
#else
    return false;
#endif
}

inline void ensureInlineEngineReady() {
    if (g_inline_engine != InlineEngine::kShadowHook) return;
#if defined(__aarch64__) || defined(__arm__)
    if (!shadowhookInit()) {
        const InlineEngine fallback = defaultInlineEngine();
        __android_log_print(ANDROID_LOG_ERROR, "ZNNloader",
                            "shadowhook unavailable in pid %d, falling back to inline=%s for this "
                            "process",
                            getpid(), inlineEngineName(fallback));
        g_inline_engine = fallback;
    }
#endif
}

inline void ensurePltEngineReady() {
    if (g_plt_engine != PltEngine::kByteHook) return;
#ifndef __riscv
    if (!bytehookInit()) {
        __android_log_print(ANDROID_LOG_ERROR, "ZNNloader",
                            "bytehook unavailable in pid %d, falling back to plt=lsplt for this "
                            "process",
                            getpid());
        g_plt_engine = PltEngine::kLsplt;
    }
#endif
}

inline bool dobbyHook(void* target, void* replacement, void** original) {
#ifndef __riscv
    if (DobbyHook(target, reinterpret_cast<dobby_dummy_func_t>(replacement),
                  reinterpret_cast<dobby_dummy_func_t*>(original)) != RS_SUCCESS) {
        __android_log_print(ANDROID_LOG_ERROR, "ZNNloader", "inlineHook %p failed", target);
        return false;
    }
    return true;
#else
    (void)target;
    (void)replacement;
    (void)original;
    return false;
#endif
}

inline bool dobbyUnhook(void* target) {
#ifndef __riscv
    return DobbyDestroy(target) == RS_SUCCESS;
#else
    (void)target;
    return false;
#endif
}

#if defined(__aarch64__) || defined(__arm__)
inline bool shadowhookHook(void* target, void* replacement, void** original) {
    if (!shadowhookInit()) return false;
    void* stub = shadowhook_hook_func_addr(target, replacement, original);
    if (!stub) {
        __android_log_print(ANDROID_LOG_ERROR, "ZNNloader", "inlineHook %p failed: %s", target,
                            shadowhook_to_errmsg(shadowhook_get_errno()));
        return false;
    }
    std::lock_guard<std::mutex> lk(g_shadowhook_mutex);
    g_shadowhook_stubs[reinterpret_cast<uintptr_t>(target)] = stub;
    return true;
}

inline bool shadowhookUnhook(void* target) {
    if (!shadowhookInit()) return false;
    void* stub = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_shadowhook_mutex);
        auto it = g_shadowhook_stubs.find(reinterpret_cast<uintptr_t>(target));
        if (it != g_shadowhook_stubs.end()) {
            stub = it->second;
            g_shadowhook_stubs.erase(it);
        }
    }
    if (!stub || shadowhook_unhook(stub) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, "ZNNloader", "inlineUnhook %p failed", target);
        return false;
    }
    return true;
}
#endif

inline bool rv64hookHook(void* target, void* replacement, void** original) {
#ifdef __riscv
    {
        rv64hook::ScopedRWXMemory rwx(target);
        if (rv64hook::InlineHook(target, replacement, original) == nullptr) {
            __android_log_print(ANDROID_LOG_ERROR, "ZNNloader", "inlineHook %p failed", target);
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

inline bool rv64hookUnhook(void* target) {
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

inline bool inlineHook(void* target, void* replacement, void** original) {
    if (!target || !replacement) return false;
    ensureInlineEngineReady();
    switch (g_inline_engine) {
        case InlineEngine::kDobby:
            return dobbyHook(target, replacement, original);
        case InlineEngine::kShadowHook:
#if defined(__aarch64__) || defined(__arm__)
            return shadowhookHook(target, replacement, original);
#else
            return false;
#endif
        case InlineEngine::kRv64Hook:
            return rv64hookHook(target, replacement, original);
    }
    return false;
}

inline bool inlineUnhook(void* target) {
    if (!target) return false;
    ensureInlineEngineReady();
    switch (g_inline_engine) {
        case InlineEngine::kDobby:
            return dobbyUnhook(target);
        case InlineEngine::kShadowHook:
#if defined(__aarch64__) || defined(__arm__)
            return shadowhookUnhook(target);
#else
            return false;
#endif
        case InlineEngine::kRv64Hook:
            return rv64hookUnhook(target);
    }
    return false;
}

#ifndef __riscv
extern "C" {
static void bytehookHookedCallback(bytehook_stub_t stub, int status_code,
                                   const char* caller_path_name, const char* sym_name,
                                   void* new_func, void* prev_func, void* arg) {
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
#endif

inline bool bytehookHook(const std::string& caller_path, const char* symbol, void* replacement,
                         void** original) {
#ifndef __riscv
    if (!bytehookInit()) return false;

    const std::string key = caller_path + "\x01" + symbol;
    {
        std::lock_guard<std::mutex> lk(g_bytehook_mutex);
        auto stub_it = g_bytehook_stubs.find(key);
        if (stub_it != g_bytehook_stubs.end()) {
            if (replacement == g_bytehook_originals[key]) {
                if (bytehook_unhook(stub_it->second) != 0) {
                    __android_log_print(ANDROID_LOG_ERROR, "ZNNloader",
                                        "pltHook %s: bytehook_unhook failed", symbol);
                    return false;
                }
                if (original) *original = g_bytehook_originals[key];
                g_bytehook_originals.erase(key);
                g_bytehook_stubs.erase(stub_it);
                return true;
            }
            __android_log_print(ANDROID_LOG_ERROR, "ZNNloader",
                                "pltHook %s: %s is already hooked, unhook first", symbol,
                                caller_path.c_str());
            return false;
        }
    }

    void* backup = nullptr;
    const bytehook_stub_t stub =
        bytehook_hook_single(caller_path.c_str(), nullptr, symbol, replacement,
                             bytehookHookedCallback, &backup);
    if (!stub) {
        __android_log_print(ANDROID_LOG_ERROR, "ZNNloader",
                            "pltHook %s: bytehook_hook_single failed for %s", symbol,
                            caller_path.c_str());
        return false;
    }
    if (!backup) {
        __android_log_print(ANDROID_LOG_ERROR, "ZNNloader", "pltHook %s: no matching relocation in %s",
                            symbol, caller_path.c_str());
        bytehook_unhook(stub);
        return false;
    }
    {
        std::lock_guard<std::mutex> lk(g_bytehook_mutex);
        g_bytehook_stubs[key] = stub;
        g_bytehook_originals[key] = backup;
    }
    if (original) *original = backup;
    return true;
#else
    (void)caller_path;
    (void)symbol;
    (void)replacement;
    (void)original;
    return false;
#endif
}

inline bool lspltHook(dev_t dev, ino_t inode, const char* symbol, void* replacement,
                      void** original) {
    void* backup = nullptr;
    if (!lsplt::RegisterHook(dev, inode, symbol, replacement, &backup)) {
        __android_log_print(ANDROID_LOG_ERROR, "ZNNloader", "pltHook %s: RegisterHook failed",
                            symbol);
        return false;
    }
    if (!lsplt::CommitHook()) {
        __android_log_print(ANDROID_LOG_ERROR, "ZNNloader", "pltHook %s: CommitHook failed",
                            symbol);
        return false;
    }
    if (!backup) {
        __android_log_print(ANDROID_LOG_ERROR, "ZNNloader", "pltHook %s: symbol not found in PLT",
                            symbol);
        return false;
    }
    if (original) *original = backup;
    return true;
}

inline const MapEntry* pltTargetEntry(const std::vector<MapEntry>& maps, uintptr_t base) {
    const MapEntry* hit = nullptr;
    for (const auto& m : maps) {
        if (m.inode == 0) continue;
        if ((base >= m.start && base < m.end) || base == m.start - m.offset) {
            hit = &m;
            break;
        }
    }
    if (!hit) return nullptr;
    for (const auto& m : maps) {
        if (m.dev == hit->dev && m.inode == hit->inode && m.offset == 0) return &m;
    }
    return hit;
}

inline bool pltHook(void* base, const char* symbol, void* replacement, void** original) {
    if (!base || !symbol || !replacement) return false;
    ensurePltEngineReady();

    const uintptr_t b = reinterpret_cast<uintptr_t>(base);
    const auto maps = parseMaps("self");
    const MapEntry* entry = pltTargetEntry(maps, b);
    if (!entry) {
        __android_log_print(ANDROID_LOG_ERROR, "ZNNloader", "pltHook %s: base %p not found in maps",
                            symbol, base);
        return false;
    }
    __android_log_print(ANDROID_LOG_INFO, "ZNNloader", "pltHook base=%p symbol=%s", base, symbol);
    __android_log_print(ANDROID_LOG_INFO, "ZNNloader", "pltHook %s: dev=%llu inode=%llu path=%s",
                        symbol, static_cast<unsigned long long>(entry->dev),
                        static_cast<unsigned long long>(entry->inode), entry->path.c_str());

    switch (g_plt_engine) {
        case PltEngine::kLsplt:
            return lspltHook(entry->dev, entry->inode, symbol, replacement, original);
        case PltEngine::kByteHook:
#ifndef __riscv
            return bytehookHook(entry->path, symbol, replacement, original);
#else
            return false;
#endif
    }
    return false;
}

}  // namespace znn::hook
