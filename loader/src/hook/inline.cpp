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

#include "hook/inline.h"

#include "hook/backend.h"
#include "hook/engine.h"

namespace znn::hook {

bool inlineHook(void* target, void* replacement, void** original) {
    if (!target || !replacement) return false;
    ensureInlineEngineReady();
    switch (inlineEngine()) {
        case InlineEngine::kDobby:
            return backend::dobbyHook(target, replacement, original);
        case InlineEngine::kShadowHook:
            return backend::shadowhookHook(target, replacement, original);
        case InlineEngine::kRv64Hook:
            return backend::rv64hookHook(target, replacement, original);
    }
    return false;
}

bool inlineUnhook(void* target) {
    if (!target) return false;
    ensureInlineEngineReady();
    switch (inlineEngine()) {
        case InlineEngine::kDobby:
            return backend::dobbyUnhook(target);
        case InlineEngine::kShadowHook:
            return backend::shadowhookUnhook(target);
        case InlineEngine::kRv64Hook:
            return backend::rv64hookUnhook(target);
    }
    return false;
}

}  //namespace znn::hook
