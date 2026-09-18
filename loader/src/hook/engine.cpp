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

#include "hook/engine.h"

#include "hook/backend.h"
#include "log.h"

#include <string.h>
#include <unistd.h>

namespace znn::hook {
namespace {

InlineEngine g_inline_engine = defaultInlineEngine();
PltEngine g_plt_engine = defaultPltEngine();

}  //namespace

const char* inlineEngineName(InlineEngine e) {
    switch (e) {
        case InlineEngine::kDobby: return "dobby";
        case InlineEngine::kShadowHook: return "shadowhook";
        case InlineEngine::kRv64Hook: return "rv64hook";
    }
    return "unknown";
}

const char* pltEngineName(PltEngine e) {
    switch (e) {
        case PltEngine::kLsplt: return "lsplt";
        case PltEngine::kByteHook: return "bytehook";
        case PltEngine::kXHook: return "xhook";
    }
    return "unknown";
}

bool inlineEngineSupported(InlineEngine e) {
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

bool pltEngineSupported(PltEngine e) {
    switch (e) {
        case PltEngine::kLsplt:
            return true;
        case PltEngine::kByteHook:
#ifndef __riscv
            return true;
#else
            return false;
#endif
        case PltEngine::kXHook:
#ifndef __riscv
            return true;
#else
            return false;
#endif
    }
    return false;
}

InlineEngine defaultInlineEngine() {
#ifdef __riscv
    return InlineEngine::kRv64Hook;
#else
    return InlineEngine::kDobby;
#endif
}

PltEngine defaultPltEngine() { return PltEngine::kLsplt; }

bool parseInlineEngine(const char* name, InlineEngine* out) {
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

bool parsePltEngine(const char* name, PltEngine* out) {
    if (!name) return false;
    PltEngine e;
    if (strcmp(name, "lsplt") == 0) {
        e = PltEngine::kLsplt;
    } else if (strcmp(name, "bytehook") == 0) {
        e = PltEngine::kByteHook;
    } else if (strcmp(name, "xhook") == 0) {
        e = PltEngine::kXHook;
    } else {
        return false;
    }
    if (!pltEngineSupported(e)) return false;
    *out = e;
    return true;
}

InlineEngine inlineEngine() { return g_inline_engine; }

PltEngine pltEngine() { return g_plt_engine; }

void setInlineEngine(InlineEngine e) {
    if (inlineEngineSupported(e)) g_inline_engine = e;
}

void setPltEngine(PltEngine e) {
    if (pltEngineSupported(e)) g_plt_engine = e;
}

void ensureInlineEngineReady() {
    if (g_inline_engine != InlineEngine::kShadowHook) return;
    if (!backend::shadowhookInit()) {
        const InlineEngine fallback = defaultInlineEngine();
        LOGE("shadowhook unavailable in pid %d, falling back to inline=%s for this process", getpid(),
             inlineEngineName(fallback));
        g_inline_engine = fallback;
    }
}

void ensurePltEngineReady() {
    if (g_plt_engine != PltEngine::kByteHook) return;
    if (!backend::bytehookInit()) {
        LOGE("bytehook unavailable in pid %d, falling back to plt=lsplt for this process", getpid());
        g_plt_engine = PltEngine::kLsplt;
    }
}

}  //namespace znn::hook
