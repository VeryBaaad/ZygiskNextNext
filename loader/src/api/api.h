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

#include "api/resolver.h"
#include "zygisk_next_api.h"

#include <cstddef>

namespace znn::api {

//hook api
int pltHook(void* base, const char* symbol, void* hook, void** original);
int inlineHook(void* target, void* addr, void** original);
int inlineUnhook(void* target);

//resolver api (v2+)
ZnSymbolResolver* newSymbolResolver(const char* path, void* base);
void freeSymbolResolver(ZnSymbolResolver* resolver);
void* getBaseAddress(ZnSymbolResolver* resolver);
void* symbolLookup(ZnSymbolResolver* resolver, const char* name, bool prefix, size_t* size);
void forEachSymbols(ZnSymbolResolver* resolver,
                    bool (*callback)(const char* name, void* addr, size_t size, void* data),
                    void* data);

//resolver stubs below v2
ZnSymbolResolver* newSymbolResolverUnavailable(const char* path, void* base);
void freeSymbolResolverUnavailable(ZnSymbolResolver* resolver);
void* getBaseAddressUnavailable(ZnSymbolResolver* resolver);
void* symbolLookupUnavailable(ZnSymbolResolver* resolver, const char* name, bool prefix,
                              size_t* size);
void forEachSymbolsUnavailable(ZnSymbolResolver* resolver,
                               bool (*callback)(const char* name, void* addr, size_t size,
                                                void* data),
                               void* data);

//companion api (v3+)
int connectCompanion(void* handle);

//companion stub below v3
int connectCompanionUnavailable(void* handle);

//runtime api (v4+)
const ZygiskNextRuntime* getRuntime();
const ZygiskNextRuntime* getRuntimeUnavailable();

//table per target api version
const ZygiskNextAPI* apiForVersion(int target_api_version);

}  //namespace znn::api
