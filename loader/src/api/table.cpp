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

namespace znn::api {
namespace {

//v1: hook only, v2: + symbol resolver, v3: + companion, v4: + runtime

const ZygiskNextAPI kApiV1 = {
    pltHook,                      inlineHook, inlineUnhook,
    newSymbolResolverUnavailable, freeSymbolResolverUnavailable,
    getBaseAddressUnavailable,    symbolLookupUnavailable,
    forEachSymbolsUnavailable,
    connectCompanionUnavailable,
    getRuntimeUnavailable,
};

const ZygiskNextAPI kApiV2 = {
    pltHook,                      inlineHook, inlineUnhook,
    newSymbolResolver,            freeSymbolResolver,
    getBaseAddress,               symbolLookup,
    forEachSymbols,
    connectCompanionUnavailable,
    getRuntimeUnavailable,
};

const ZygiskNextAPI kApiV3 = {
    pltHook,           inlineHook,   inlineUnhook,
    newSymbolResolver, freeSymbolResolver, getBaseAddress,
    symbolLookup,      forEachSymbols,
    connectCompanion,
    getRuntimeUnavailable,
};

const ZygiskNextAPI kApiV4 = {
    pltHook,           inlineHook,   inlineUnhook,
    newSymbolResolver, freeSymbolResolver, getBaseAddress,
    symbolLookup,      forEachSymbols,
    connectCompanion,
    getRuntime,
};

}  //namespace

const ZygiskNextAPI* apiForVersion(int target_api_version) {
    if (target_api_version >= 4) return &kApiV4;
    if (target_api_version == 3) return &kApiV3;
    if (target_api_version == 2) return &kApiV2;
    if (target_api_version == 1) return &kApiV1;
    return &kApiV4;
}

}  //namespace znn::api
