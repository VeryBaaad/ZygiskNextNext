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

#include "config/hooks.h"

#include "hook/engine.h"
#include "ipc/client.h"
#include "ipc/protocol.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>

#include <string>

namespace znn::config {
namespace {

bool readConfigFile(std::string& inline_out, std::string& plt_out) {
    FILE* f = fopen(ipc::kConfigPath, "re");
    if (!f) return false;
    char* line = nullptr;
    size_t cap = 0;
    while (getline(&line, &cap, f) > 0) {
        std::string l = line;
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        size_t eq = l.find('=');
        if (eq == std::string::npos || eq == 0) continue;
        const std::string key = l.substr(0, eq);
        const std::string val = l.substr(eq + 1);
        if (key == "inline_hook") inline_out = val;
        else if (key == "plt_hook") plt_out = val;
    }
    free(line);
    fclose(f);
    return !inline_out.empty() || !plt_out.empty();
}

}  //namespace

void resolveHookEngines() {
    hook::InlineEngine inline_engine = hook::defaultInlineEngine();
    hook::PltEngine plt_engine = hook::defaultPltEngine();

    std::string inline_name, plt_name;
    if (ipc::hookConfig(inline_name, plt_name) || readConfigFile(inline_name, plt_name)) {
        hook::InlineEngine parsed_inline;
        hook::PltEngine parsed_plt;
        if (hook::parseInlineEngine(inline_name.c_str(), &parsed_inline)) inline_engine = parsed_inline;
        if (hook::parsePltEngine(plt_name.c_str(), &parsed_plt)) plt_engine = parsed_plt;
    }

    hook::setInlineEngine(inline_engine);
    hook::setPltEngine(plt_engine);
    LOGI("hook engines: inline=%s plt=%s", hook::inlineEngineName(inline_engine),
         hook::pltEngineName(plt_engine));
}

}  //namespace znn::config
