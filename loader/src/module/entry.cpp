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

#include "module/entry.h"

#include "process/self.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

namespace znn::module {

std::vector<Entry> parseManifest(const std::string& moddir, const std::string& file) {
    std::vector<Entry> out;
    FILE* f = fopen(file.c_str(), "re");
    if (!f) return out;

    char* line = nullptr;
    size_t cap = 0;
    while (getline(&line, &cap, f) > 0) {
        std::string l = line;

        std::vector<std::string> toks;
        size_t i = 0;
        while (i < l.size()) {
            while (i < l.size() && isspace(static_cast<unsigned char>(l[i]))) ++i;
            size_t j = i;
            while (j < l.size() && !isspace(static_cast<unsigned char>(l[j]))) ++j;
            if (j > i) toks.push_back(l.substr(i, j - i));
            i = j;
        }
        if (toks.size() < 2) continue;

        Entry e;
        e.dir = moddir;

        if (toks[0].rfind("path=", 0) == 0) {
            e.is_name = false;
            e.target = toks[0].substr(5);
        } else if (toks[0].rfind("name=", 0) == 0) {
            e.is_name = true;
            e.target = toks[0].substr(5);
        } else {
            continue;
        }

        for (size_t k = 1; k + 1 < toks.size(); ++k) {
            if (toks[k] == "companion") e.companion = true;
        }
        e.lib = toks.back();
        out.push_back(std::move(e));
    }
    free(line);
    fclose(f);
    return out;
}

bool matches(const Entry& e) {
    if (e.is_name) return process::exeName() == e.target;
    return process::exePath() == e.target;
}

bool resolveLibPath(const Entry& e, std::string& out) {
    std::string candidate = e.lib;
    if (candidate.empty() || candidate[0] != '/') candidate = e.dir + "/" + candidate;

    char real_mod[PATH_MAX];
    char real_lib[PATH_MAX];
    if (!realpath(e.dir.c_str(), real_mod)) return false;
    if (!realpath(candidate.c_str(), real_lib)) return false;

    const std::string mod = real_mod;
    const std::string lib = real_lib;
    if (lib.size() <= mod.size() || lib.compare(0, mod.size(), mod) != 0 || lib[mod.size()] != '/') {
        return false;
    }
    out = lib;
    return true;
}

}  //namespace znn::module
