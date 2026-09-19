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

#include "hyos/hooks.h"

#include "hook/inline.h"
#include "hook/plt.h"
#include "hyos/runtime.h"
#include "hyos/state.h"
#include "log.h"
#include "utils/elf_util.h"

#include <dlfcn.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

namespace znn::hyos {
namespace {

//spawner image, offset-0 mapping
void findSpawnerImage() {
    State& s = state();
    if (s.spawner.inode != 0) return;
    for (const auto& m : parseMaps("self")) {
        if (m.offset != 0 || m.inode == 0) continue;
        if (m.path.find("hyos_spawner") == std::string::npos) continue;
        s.spawner.dev = m.dev;
        s.spawner.inode = m.inode;
        s.spawner.base = m.start;
        LOGI("HYOS: spawner image dev=%llu inode=%llu base=%p",
             static_cast<unsigned long long>(m.dev),
             static_cast<unsigned long long>(m.inode), reinterpret_cast<void*>(m.start));
        break;
    }
}

int setcontextHook(uid_t uid, int is_system_server, const char* se_info, const char* pkg_name) {
    State& s = state();
    int ret = s.orig_setcontext ? s.orig_setcontext(uid, is_system_server, se_info, pkg_name) : -1;
    if (ret == 0) {
        deliverAppSpecialized(pkg_name, se_info);
    } else if (!s.fired && s.in_child) {
        LOGW("HYOS: selinux_android_setcontext failed (%d)", ret);
    }
    return ret;
}

int setnameHook(pthread_t thread, const char* name) {
    State& s = state();
    int ret = s.orig_setname ? s.orig_setname(thread, name) : -1;
    if (ret == 0 && !s.has_captured_name && s.in_child && !s.fired && thread == pthread_self() &&
        name && name[0] != '\0') {
        strlcpy(s.captured_name, name, sizeof(s.captured_name));
        s.has_captured_name = true;
        LOGI("HYOS: captured process name %s", s.captured_name);
    }
    return ret;
}

pid_t forkHook() {
    State& s = state();
    pid_t res = s.orig_fork ? s.orig_fork() : -1;
    if (res == 0) {
        atForkChild();
    }
    return res;
}

}  //namespace

//no allocation here
void atForkChild() {
    State& s = state();
    s.in_child = true;
    s.fired = false;
    s.has_captured_name = false;
    s.captured_name[0] = '\0';
}

void atForkPrepare() {
    State& s = state();
    if (s.module_count > 0 && (!s.orig_setcontext || !s.orig_setname)) {
        installHooks();
    }
}

void installHooks() {
    State& s = state();
    findSpawnerImage();

    if (s.spawner.inode != 0 && !s.plt_fork_tried) {
        s.plt_fork_tried = true;
        if (!s.orig_fork) {
            void* backup = nullptr;
            if (hook::pltHook(reinterpret_cast<void*>(s.spawner.base), "fork",
                              reinterpret_cast<void*>(forkHook), &backup)) {
                s.orig_fork = reinterpret_cast<ForkFn>(backup);
                LOGI("HYOS: PLT hooked fork");
            }
        }
    }
    if (s.spawner.inode != 0 && !s.plt_ctx_tried) {
        s.plt_ctx_tried = true;
        if (!s.orig_setcontext) {
            void* backup = nullptr;
            if (hook::pltHook(reinterpret_cast<void*>(s.spawner.base),
                              "selinux_android_setcontext",
                              reinterpret_cast<void*>(setcontextHook), &backup)) {
                s.orig_setcontext = reinterpret_cast<SetcontextFn>(backup);
                LOGI("HYOS: PLT hooked selinux_android_setcontext");
            }
        }
    }

    if (!s.orig_setcontext) {
        void* fn = dlsym(RTLD_DEFAULT, "selinux_android_setcontext");
        if (!fn) {
            void* h = dlopen("libselinux.so", RTLD_NOW);
            if (h) fn = dlsym(h, "selinux_android_setcontext");
        }
        if (fn && hook::inlineHook(fn, reinterpret_cast<void*>(setcontextHook),
                                   reinterpret_cast<void**>(&s.orig_setcontext))) {
            LOGI("HYOS: inline hooked selinux_android_setcontext");
        }
    }
    if (!s.orig_setname) {
        void* fn = dlsym(RTLD_DEFAULT, "pthread_setname_np");
        if (!fn) {
            void* h = dlopen("libc.so", RTLD_NOW);
            if (h) fn = dlsym(h, "pthread_setname_np");
        }
        if (fn && hook::inlineHook(fn, reinterpret_cast<void*>(setnameHook),
                                   reinterpret_cast<void**>(&s.orig_setname))) {
            LOGI("HYOS: inline hooked pthread_setname_np");
        }
    }
    if (!s.orig_setcontext && !s.warned) {
        s.warned = true;
        LOGW("HYOS: selinux_android_setcontext not available, onAppSpecialized will not fire");
    }
}

}  //namespace znn::hyos
