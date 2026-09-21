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
#include "log.h"

namespace znn::config {

void resolveHookEngines(const std::string& inline_name, const std::string& plt_name) {
    hook::InlineEngine inline_engine = hook::defaultInlineEngine();
    hook::PltEngine plt_engine = hook::defaultPltEngine();

    hook::InlineEngine parsed_inline;
    hook::PltEngine parsed_plt;
    if (hook::parseInlineEngine(inline_name.c_str(), &parsed_inline)) inline_engine = parsed_inline;
    if (hook::parsePltEngine(plt_name.c_str(), &parsed_plt)) plt_engine = parsed_plt;

    hook::setInlineEngine(inline_engine);
    hook::setPltEngine(plt_engine);
    LOGI("hook engines: inline=%s plt=%s", hook::inlineEngineName(inline_engine),
         hook::pltEngineName(plt_engine));
}

}  //namespace znn::config
