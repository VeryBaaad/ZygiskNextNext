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

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace znn::ipc {

constexpr uint8_t kChannelConnect = 1;

constexpr int kChannelBacklog = 4;
constexpr int kChannelAcceptTimeoutMs = 2000;

int bindChannel(uint32_t nonce, uint32_t index);
int acceptChannel(int listen_fd, uint32_t nonce, int timeout_ms);

bool makePeerPair(int* out_module_fd, int* out_helper_fd);

bool sendFrame(int fd, uint8_t command, int payload_fd);
bool recvFrame(int fd, uint8_t* command, int* out_fd);

}  //namespace znn::ipc
