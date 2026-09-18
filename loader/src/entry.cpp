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
#include "hyos/runtime.h"
#include "log.h"
#include "module/loader.h"
#include "process/self.h"

#include <unistd.h>

extern "C" __attribute__((visibility("default"))) void znn_loader_init() {
    LOGI("loader initialized in pid %d (%s)", getpid(), znn::process::exePath().c_str());

    znn::config::resolveHookEngines();

    if (znn::process::exeName() == "hyos_spawner") znn::hyos::setActive(true);

    znn::module::loadAll();
}
