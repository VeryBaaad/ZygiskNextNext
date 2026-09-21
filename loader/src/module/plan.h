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

#include <stdint.h>

#include <string>
#include <vector>

namespace znn::module {

struct PlanModule {
    int fd = -1;
    bool companion = false;
    uint32_t index = 0;
    std::string path;
};

struct Plan {
    std::vector<PlanModule> modules;
    std::string inline_engine;
    std::string plt_engine;
    uint32_t nonce = 0;
};

bool parsePlan(const void* raw, Plan& out);

}  //namespace znn::module
