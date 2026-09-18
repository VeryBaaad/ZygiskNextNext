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

#include "companion/process.h"

#include "ipc/protocol.h"
#include "ipc/socket.h"
#include "log.h"
#include "utils/dlext.h"
#include "zygisk_next_api.h"

#include <dlfcn.h>
#include <unistd.h>

namespace znn::companion {

[[noreturn]] void run(const char* lib_path, int ctl_fd) {
    void* lib = dlopenMemfd(lib_path, RTLD_NOW);
    if (!lib) {
        LOGE("companion: dlopen %s failed: %s", lib_path, dlerror());
        _exit(1);
    }

    auto* m = reinterpret_cast<ZygiskNextCompanionModule*>(dlsym(lib, "zn_companion_module"));
    if (!m || !m->onCompanionLoaded || !m->onModuleConnected) {
        LOGE("companion: %s does not export zn_companion_module", lib_path);
        _exit(1);
    }

    m->onCompanionLoaded();

    for (;;) {
        char cmd = 0;
        int fd = -1;
        if (!ipc::recvFull(ctl_fd, &cmd, sizeof(cmd), &fd)) break;
        if (cmd != ipc::kCmdConnectPeer) {
            if (fd >= 0) close(fd);
            continue;
        }
        if (fd >= 0) {
            m->onModuleConnected(fd);
        }
    }
    _exit(0);
}

}  //namespace znn::companion
