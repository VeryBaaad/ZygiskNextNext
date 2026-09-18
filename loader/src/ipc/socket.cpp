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

#include "ipc/socket.h"

#include "ipc/protocol.h"

#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace znn::ipc {

int connectDaemon() {
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    strlcpy(addr.sun_path, kSocketPath, sizeof(addr.sun_path));
    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }

    struct timeval tv {};
    tv.tv_sec = 5;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    return fd;
}

bool writeAll(int fd, const void* buf, size_t len) {
    const auto* p = static_cast<const char*>(buf);
    size_t off = 0;
    while (off < len) {
        const ssize_t n = write(fd, p + off, len - off);
        if (n <= 0) return false;
        off += static_cast<size_t>(n);
    }
    return true;
}

bool recvFull(int fd, void* buf, size_t len, int* out_fd) {
    auto* p = static_cast<char*>(buf);
    size_t off = 0;
    while (off < len) {
        char cmsg_buf[CMSG_SPACE(sizeof(int))] = {0};
        struct iovec iov {p + off, len - off};
        struct msghdr msg {};
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        if (out_fd) {
            msg.msg_control = cmsg_buf;
            msg.msg_controllen = sizeof(cmsg_buf);
        }
        const ssize_t n = recvmsg(fd, &msg, 0);
        if (n <= 0) return false;
        if (out_fd) {
            for (struct cmsghdr* c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
                if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS) {
                    memcpy(out_fd, CMSG_DATA(c), sizeof(int));
                    break;
                }
            }
            out_fd = nullptr;
        }
        off += static_cast<size_t>(n);
    }
    return true;
}

bool recvString(int fd, std::string& out, size_t max_len) {
    uint32_t len = 0;
    if (!recvFull(fd, &len, sizeof(len), nullptr) || len == 0 || len > max_len) return false;
    out.assign(len, '\0');
    if (!recvFull(fd, &out[0], len, nullptr) || out[len - 1] != '\0') return false;
    out.resize(len - 1);
    return true;
}

bool sendFd(int fd, const void* buf, size_t len, int payload_fd) {
    struct iovec iov {};
    iov.iov_base = const_cast<void*>(buf);
    iov.iov_len = len;

    char cmsg_buf[CMSG_SPACE(sizeof(int))] = {0};
    struct msghdr msg {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &payload_fd, sizeof(int));
    msg.msg_controllen = cmsg->cmsg_len;

    return sendmsg(fd, &msg, 0) >= 0;
}

}  //namespace znn::ipc
