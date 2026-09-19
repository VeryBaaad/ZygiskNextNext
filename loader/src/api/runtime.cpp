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

#include "api/api.h"

#include "hyos/runtime.h"
#include "log.h"

namespace znn::api {

const ZygiskNextRuntime* getRuntime() {
    return hyos::active() ? hyos::runtime() : nullptr;
}

const ZygiskNextRuntime* getRuntimeUnavailable() {
    LOGE("Runtime API requires module API version >= 4 (getRuntime)");
    return nullptr;
}

}  //namespace znn::api
