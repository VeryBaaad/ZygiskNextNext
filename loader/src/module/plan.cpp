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

#include "module/plan.h"

#include "ipc/plan.h"
#include "log.h"

#include <string.h>

namespace znn::module {
namespace {

constexpr size_t kMinPlanBytes = sizeof(ipc::Plan);

bool blobString(const char* blob, size_t blob_size, ipc::PlanString ref, std::string& out) {
    if (ref.length == 0) return true;
    const size_t end = static_cast<size_t>(ref.offset) + ref.length;
    if (end + 1 > blob_size) return false;
    if (blob[end] != '\0') return false;
    out.assign(blob + ref.offset, ref.length);
    return true;
}

}  //namespace

bool parsePlan(const void* raw, Plan& out) {
    if (!raw) return false;

    ipc::Plan header {};
    memcpy(&header, raw, sizeof(header));
    if (header.magic != ipc::kPlanMagic) {
        LOGE("boot plan: bad magic %#x", header.magic);
        return false;
    }
    if (header.version != ipc::kPlanVersion) {
        LOGE("boot plan: unsupported version %u", header.version);
        return false;
    }
    if (header.size < kMinPlanBytes || header.size > ipc::kPlanMaxBytes) {
        LOGE("boot plan: implausible size %u", header.size);
        return false;
    }
    if (header.module_count > ipc::kPlanMaxModules) {
        LOGE("boot plan: %u modules is more than we accept", header.module_count);
        return false;
    }

    const size_t records = static_cast<size_t>(header.module_count) * ipc::kPlanModuleSize;
    if (kMinPlanBytes + records > header.size) {
        LOGE("boot plan: %u modules do not fit in %u bytes", header.module_count, header.size);
        return false;
    }

    const auto* base = static_cast<const char*>(raw);
    const char* blob = base + kMinPlanBytes + records;
    const size_t blob_size = header.size - kMinPlanBytes - records;

    out = Plan();
    out.nonce = header.nonce;
    if (!blobString(blob, blob_size, header.inline_engine, out.inline_engine) ||
        !blobString(blob, blob_size, header.plt_engine, out.plt_engine)) {
        LOGE("boot plan: hook engine names are out of bounds");
        return false;
    }

    const auto* mods = reinterpret_cast<const ipc::PlanModule*>(base + kMinPlanBytes);
    out.modules.reserve(header.module_count);
    for (uint32_t i = 0; i < header.module_count; ++i) {
        PlanModule m;
        m.fd = mods[i].fd;
        m.companion = (mods[i].flags & ipc::kPlanModuleCompanion) != 0;
        m.index = mods[i].index;
        if (m.fd < 0) {
            LOGE("boot plan: module %u has no library descriptor", i);
            return false;
        }
        if (m.index >= ipc::kPlanMaxModules) {
            LOGE("boot plan: module %u has an implausible index %u", i, m.index);
            return false;
        }
        if (!blobString(blob, blob_size, mods[i].path, m.path)) {
            LOGE("boot plan: module %u path is out of bounds", i);
            return false;
        }
        out.modules.push_back(std::move(m));
    }
    return true;
}

}  //namespace znn::module
