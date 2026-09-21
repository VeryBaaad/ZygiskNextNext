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

constexpr uint32_t kPlanMagic = 0x5A4E4E31;  //"ZNN1"
constexpr uint32_t kPlanVersion = 1;

constexpr uint32_t kPlanModuleCompanion = 1u << 0;

constexpr size_t kPlanMaxModules = 32;
constexpr size_t kPlanMaxBytes = 32 * 1024;

constexpr size_t kChannelNameMax = 96;

struct PlanString {
    uint32_t offset;
    uint32_t length;
};

struct PlanModule {
    int32_t fd;
    uint32_t flags;
    uint32_t index;
    PlanString path;
};

struct Plan {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t module_count;
    uint32_t nonce;
    PlanString inline_engine;
    PlanString plt_engine;
};

constexpr size_t kPlanHeaderSize = sizeof(Plan);
constexpr size_t kPlanModuleSize = sizeof(PlanModule);

static_assert(kPlanHeaderSize == 36, "the boot plan header must stay 36 bytes");
static_assert(kPlanModuleSize == 20, "a boot plan module must stay 20 bytes");

size_t channelName(uint32_t nonce, uint32_t index, char* out);

}  //namespace znn::ipc
