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

#include "utils/dlext.h"

#include "log.h"
#include "utils/elf_util.h"

#include <android/dlext.h>
#include <dlfcn.h>

namespace znn {
namespace {

using DlopenExtFn = void* (*)(const char*, int, const android_dlextinfo*);

DlopenExtFn resolveDlopenExt() {
    const char* linkers[] = {"linker64", "linker"};
    for (const char* linker : linkers) {
        const uintptr_t address = resolveLibrarySymbol(linker, "__loader_android_dlopen_ext");
        if (address != 0) return reinterpret_cast<DlopenExtFn>(address);
    }
    if (void* symbol = dlsym(RTLD_DEFAULT, "android_dlopen_ext")) {
        return reinterpret_cast<DlopenExtFn>(symbol);
    }
    return nullptr;
}

}  //namespace

void* dlopenFd(int fd, const char* name, int flags) {
    const DlopenExtFn dlopen_ext = resolveDlopenExt();
    if (!dlopen_ext) {
        LOGE("dlopen %s: this process has no android_dlopen_ext", name);
        return nullptr;
    }

    android_dlextinfo ext = {};
    ext.flags = ANDROID_DLEXT_USE_LIBRARY_FD;
    ext.library_fd = fd;
    return dlopen_ext(name, flags, &ext);
}

}  //namespace znn
