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

#include "api/api.h"

#include "ipc/protocol.h"
#include "ipc/socket.h"
#include "log.h"
#include "module/handle.h"

#include <sys/socket.h>
#include <unistd.h>

namespace znn::api {

int connectCompanion(void* handle) {
    auto* h = static_cast<ModuleHandle*>(handle);
    if (!h || h->companion_fd < 0) return -1;

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) != 0) return -1;

    const char cmd = ipc::kCmdConnectPeer;
    if (!ipc::sendFd(h->companion_fd, &cmd, sizeof(cmd), sv[1])) {
        close(sv[0]);
        close(sv[1]);
        return -1;
    }
    close(sv[1]);
    return sv[0];
}

int connectCompanionUnavailable(void*) {
    LOGE("Companion API requires module API version >= 3 (connectCompanion)");
    return -1;
}

}  //namespace znn::api
