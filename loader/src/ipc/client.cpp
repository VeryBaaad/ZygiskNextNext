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

#include "ipc/client.h"

#include "ipc/protocol.h"
#include "ipc/socket.h"
#include "log.h"

#include <unistd.h>

#include <utility>

namespace znn::ipc {

int spawnCompanion(const std::string& lib_path) {
    const int fd = connectDaemon();
    if (fd < 0) return -1;

    const uint32_t magic = kRequestMagic;
    const uint32_t cmd = kCmdSpawnCompanion;
    const uint32_t plen = static_cast<uint32_t>(lib_path.size() + 1);
    if (!writeAll(fd, &magic, sizeof(magic)) || !writeAll(fd, &cmd, sizeof(cmd)) ||
        !writeAll(fd, &plen, sizeof(plen)) || !writeAll(fd, lib_path.c_str(), plen)) {
        close(fd);
        return -1;
    }

    uint32_t ack = 0;
    int cfd = -1;
    recvFull(fd, &ack, sizeof(ack), &cfd);
    close(fd);
    return cfd;
}

bool listModules(const std::string& process_name, const std::string& process_path,
                 std::vector<DaemonModule>& out) {
    const int fd = connectDaemon();
    if (fd < 0) return false;

    const uint32_t magic = kRequestMagic;
    const uint32_t cmd = kCmdListModules;
    const uint32_t nlen = static_cast<uint32_t>(process_name.size() + 1);
    const uint32_t plen = static_cast<uint32_t>(process_path.size() + 1);
    if (!writeAll(fd, &magic, sizeof(magic)) || !writeAll(fd, &cmd, sizeof(cmd)) ||
        !writeAll(fd, &nlen, sizeof(nlen)) || !writeAll(fd, process_name.c_str(), nlen) ||
        !writeAll(fd, &plen, sizeof(plen)) || !writeAll(fd, process_path.c_str(), plen)) {
        close(fd);
        return false;
    }

    for (;;) {
        uint32_t rlen = 0;
        int mfd = -1;
        if (!recvFull(fd, &rlen, sizeof(rlen), &mfd)) {
            close(fd);
            return false;
        }
        if (rlen == 0) break;
        if (rlen > kMaxLibPathLen) {
            close(fd);
            return false;
        }
        std::string lib(rlen, '\0');
        uint32_t comp = 0;
        if (!recvFull(fd, &lib[0], rlen, nullptr) ||
            !recvFull(fd, &comp, sizeof(comp), nullptr) || lib[rlen - 1] != '\0' || mfd < 0) {
            close(fd);
            return false;
        }
        lib.resize(rlen - 1);
        out.push_back({std::move(lib), comp != 0, mfd});
    }
    close(fd);
    return true;
}

bool hookConfig(std::string& inline_out, std::string& plt_out) {
    const int fd = connectDaemon();
    if (fd < 0) return false;

    const uint32_t magic = kRequestMagic;
    const uint32_t cmd = kCmdHookConfig;
    const bool ok = writeAll(fd, &magic, sizeof(magic)) && writeAll(fd, &cmd, sizeof(cmd)) &&
                    recvString(fd, inline_out, kMaxConfigValueLen) &&
                    recvString(fd, plt_out, kMaxConfigValueLen);
    close(fd);
    return ok;
}

}  //namespace znn::ipc
