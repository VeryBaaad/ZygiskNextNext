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

#include <vector>

namespace znn::debugdata {

enum class Backend {
    kLzma,
    kXz,
};

Backend backend();

void setBackend(Backend backend);

bool parseBackend(const char* name, Backend* out);

const char* backendName(Backend backend);

bool decompress(const uint8_t* data, size_t size, std::vector<uint8_t>& out);

}  //namespace znn::debugdata
