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

#include "ipc/channel.h"

#include "ipc/plan.h"
#include "log.h"

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace znn::ipc {
namespace {

constexpr int kNonceBytes = 4;

void closeFd(int fd) {
    if (fd >= 0) close(fd);
}

bool isRootPeer(int fd) {
    struct ucred peer {};
    socklen_t size = sizeof(peer);
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &peer, &size) != 0) return false;
    if (peer.uid != 0) {
        LOGW("companion channel: rejected peer uid %u", static_cast<unsigned>(peer.uid));
        return false;
    }
    return true;
}

bool waitReadable(int fd, int timeout_ms) {
    struct pollfd pfd {};
    pfd.fd = fd;
    pfd.events = POLLIN;
    for (;;) {
        const int rc = poll(&pfd, 1, timeout_ms);
        if (rc > 0) return true;
        if (rc < 0 && errno == EINTR) continue;
        return false;
    }
}

}  //namespace

size_t channelName(uint32_t nonce, uint32_t index, char* out) {
    out[0] = '\0';
    const int written = snprintf(out + 1, kChannelNameMax - 1, "znn.%08x.%u", nonce, index);
    if (written <= 0) return 1;
    return static_cast<size_t>(written) + 1;
}

int bindChannel(uint32_t nonce, uint32_t index) {
    const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        LOGE("companion channel %u: socket failed: %s", index, strerror(errno));
        return -1;
    }

    struct sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    const size_t name_len = channelName(nonce, index, addr.sun_path);
    const socklen_t addr_len = static_cast<socklen_t>(offsetof(struct sockaddr_un, sun_path) + name_len);
    if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr), addr_len) != 0) {
        LOGE("companion channel %u: bind failed: %s", index, strerror(errno));
        close(fd);
        return -1;
    }
    if (listen(fd, kChannelBacklog) != 0) {
        LOGE("companion channel %u: listen failed: %s", index, strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

int acceptChannel(int listen_fd, uint32_t nonce, int timeout_ms) {
    if (listen_fd < 0) return -1;
    if (!waitReadable(listen_fd, timeout_ms)) {
        LOGW("companion channel: no helper connected within %d ms", timeout_ms);
        return -1;
    }

    const int fd = accept4(listen_fd, nullptr, nullptr, SOCK_CLOEXEC);
    if (fd < 0) {
        LOGW("companion channel: accept failed: %s", strerror(errno));
        return -1;
    }
    if (!isRootPeer(fd)) {
        close(fd);
        return -1;
    }

    uint8_t proof[kNonceBytes] = {};
    if (!waitReadable(fd, kChannelAcceptTimeoutMs)) {
        LOGW("companion channel: helper did not send its nonce");
        close(fd);
        return -1;
    }
    size_t off = 0;
    while (off < sizeof(proof)) {
        const ssize_t n = read(fd, proof + off, sizeof(proof) - off);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) {
            LOGW("companion channel: cannot read the nonce: %s", strerror(errno));
            close(fd);
            return -1;
        }
        off += static_cast<size_t>(n);
    }
    if (memcmp(proof, &nonce, sizeof(proof)) != 0) {
        LOGW("companion channel: nonce mismatch, dropping the connection");
        close(fd);
        return -1;
    }
    return fd;
}

bool makePeerPair(int* out_module_fd, int* out_helper_fd) {
    int sv[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) != 0) {
        LOGW("companion: socketpair failed: %s", strerror(errno));
        return false;
    }
    *out_module_fd = sv[0];
    *out_helper_fd = sv[1];
    return true;
}

bool sendFrame(int fd, uint8_t command, int payload_fd) {
    struct iovec iov {};
    iov.iov_base = &command;
    iov.iov_len = sizeof(command);

    char cmsg_buf[CMSG_SPACE(sizeof(int))] = {};
    struct msghdr msg {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    if (payload_fd >= 0) {
        msg.msg_control = cmsg_buf;
        msg.msg_controllen = sizeof(cmsg_buf);
        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        memcpy(CMSG_DATA(cmsg), &payload_fd, sizeof(int));
        msg.msg_controllen = cmsg->cmsg_len;
    }

    for (;;) {
        const ssize_t n = sendmsg(fd, &msg, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        return n > 0;
    }
}

bool recvFrame(int fd, uint8_t* command, int* out_fd) {
    char cmsg_buf[CMSG_SPACE(sizeof(int))] = {};
    struct iovec iov {};
    iov.iov_base = command;
    iov.iov_len = sizeof(*command);
    struct msghdr msg {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    ssize_t n;
    for (;;) {
        n = recvmsg(fd, &msg, 0);
        if (n < 0 && errno == EINTR) continue;
        break;
    }
    if (n <= 0) return false;

    int received = -1;
    for (struct cmsghdr* c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
        if (c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_RIGHTS) continue;
        if (c->cmsg_len < CMSG_LEN(sizeof(int))) continue;
        memcpy(&received, CMSG_DATA(c), sizeof(int));
        break;
    }
    if (out_fd) *out_fd = received;
    else closeFd(received);
    return true;
}

}  //namespace znn::ipc
