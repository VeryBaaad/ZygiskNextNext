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

#include "ipc/channel.h"
#include "log.h"
#include "module/handle.h"

#include <unistd.h>

namespace znn::api {

int connectCompanion(void* handle) {
    auto* h = static_cast<ModuleHandle*>(handle);
    if (!h || h->companion_listen_fd < 0) return -1;

    if (h->companion_channel < 0) {
        h->companion_channel =
            ipc::acceptChannel(h->companion_listen_fd, h->companion_nonce,
                               ipc::kChannelAcceptTimeoutMs);
    }
    if (h->companion_channel < 0) return -1;

    int module_fd = -1;
    int helper_fd = -1;
    if (!ipc::makePeerPair(&module_fd, &helper_fd)) return -1;
    if (!ipc::sendFrame(h->companion_channel, ipc::kChannelConnect, helper_fd)) {
        close(module_fd);
        close(helper_fd);
        return -1;
    }
    close(helper_fd);
    return module_fd;
}

int connectCompanionUnavailable(void*) {
    LOGE("Companion API requires module API version >= 3 (connectCompanion)");
    return -1;
}

}  //namespace znn::api
