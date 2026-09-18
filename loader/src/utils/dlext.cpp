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

#include <android/dlext.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/memfd.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace znn {

void* dlopenMemfd(const char* path, int flags) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        LOGE("dlopen %s: cannot open: %s", path, strerror(errno));
        return nullptr;
    }

    int memfd = static_cast<int>(syscall(SYS_memfd_create, "znn-module", MFD_CLOEXEC));
    if (memfd < 0) {
        LOGE("dlopen %s: memfd_create failed: %s", path, strerror(errno));
        close(fd);
        return nullptr;
    }

    char buf[16384];
    ssize_t n;
    bool ok = true;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        ssize_t left = n;
        const char* p = buf;
        while (left > 0) {
            ssize_t w = write(memfd, p, static_cast<size_t>(left));
            if (w <= 0) {
                ok = false;
                break;
            }
            p += w;
            left -= w;
        }
        if (!ok) break;
    }
    close(fd);
    if (!ok || n < 0) {
        LOGE("dlopen %s: copy to memfd failed: %s", path, strerror(errno));
        close(memfd);
        return nullptr;
    }

    android_dlextinfo extinfo = {};
    extinfo.flags = ANDROID_DLEXT_USE_LIBRARY_FD;
    extinfo.library_fd = memfd;

    void* lib = android_dlopen_ext(path, flags, &extinfo);
    if (!lib) LOGE("dlopen %s via memfd failed: %s", path, dlerror());
    //memfd kept open for bionic
    return lib;
}

void* dlopenFd(int fd, const char* name, int flags) {
    android_dlextinfo ext = {};
    ext.flags = ANDROID_DLEXT_USE_LIBRARY_FD;
    ext.library_fd = fd;
    return android_dlopen_ext(name, flags, &ext);
}

}  //namespace znn
