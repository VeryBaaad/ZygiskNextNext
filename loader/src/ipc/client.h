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

#include <string>
#include <vector>

namespace znn::ipc {

struct DaemonModule {
    std::string lib_path;
    bool companion = false;
    int fd = -1;
};

int spawnCompanion(const std::string& lib_path);

bool listModules(const std::string& process_name, const std::string& process_path,
                 std::vector<DaemonModule>& out);

bool hookConfig(std::string& inline_out, std::string& plt_out);

}  //namespace znn::ipc
