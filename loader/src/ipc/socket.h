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

#include <cstddef>
#include <string>

namespace znn::ipc {

int connectDaemon();

bool writeAll(int fd, const void* buf, size_t len);

bool recvFull(int fd, void* buf, size_t len, int* out_fd);

bool recvString(int fd, std::string& out, size_t max_len);

bool sendFd(int fd, const void* buf, size_t len, int payload_fd);

}  //namespace znn::ipc
