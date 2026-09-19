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

#include "process/self.h"

#include <limits.h>
#include <string.h>
#include <unistd.h>

namespace znn::process {

std::string exePath() {
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return {};
    std::string p(buf, static_cast<size_t>(n));
    constexpr char kDeleted[] = " (deleted)";
    constexpr size_t kDeletedLen = sizeof(kDeleted) - 1;
    if (p.size() > kDeletedLen && p.compare(p.size() - kDeletedLen, kDeletedLen, kDeleted) == 0) {
        p.resize(p.size() - kDeletedLen);
    }
    return p;
}

std::string exeName() {
    const std::string p = exePath();
    const auto pos = p.rfind('/');
    return pos == std::string::npos ? p : p.substr(pos + 1);
}

}  //namespace znn::process
