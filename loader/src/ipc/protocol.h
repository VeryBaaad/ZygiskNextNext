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

#include <cstdint>

namespace znn::ipc {

//daemon control socket, config file
constexpr char kSocketPath[] = "/data/adb/zygisknextsu/companion.sock";
constexpr char kConfigPath[] = "/data/adb/zygisknextsu/config";

constexpr uint32_t kRequestMagic = 0x5A4E4E43;  //"ZNNC"
constexpr uint32_t kCmdSpawnCompanion = 1;
constexpr uint32_t kCmdListModules = 2;
constexpr uint32_t kCmdHookConfig = 3;

constexpr char kCmdConnectPeer = 1;

constexpr uint32_t kMaxLibPathLen = 4096;
constexpr uint32_t kMaxConfigValueLen = 64;

}  //namespace znn::ipc
