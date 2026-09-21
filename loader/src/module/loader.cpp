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

#include "module/loader.h"

#include "api/api.h"
#include "ipc/channel.h"
#include "log.h"
#include "module/handle.h"
#include "utils/dlext.h"

#include <dlfcn.h>

namespace znn::module {
namespace {

int openCompanionChannel(const Plan& plan, const PlanModule& module) {
    const int fd = ipc::bindChannel(plan.nonce, module.index);
    if (fd < 0) {
        LOGE("module %s: cannot open its companion channel", module.path.c_str());
    }
    return fd;
}

void loadEntry(const Plan& plan, const PlanModule& entry) {
    void* lib = dlopenFd(entry.fd, entry.path.c_str(), RTLD_NOW);
    if (!lib) {
        LOGE("dlopen %s (fd %d) failed: %s", entry.path.c_str(), entry.fd, dlerror());
        return;
    }

    auto* m = reinterpret_cast<ZygiskNextModule*>(dlsym(lib, "zn_module"));
    if (!m || !m->onModuleLoaded) {
        LOGE("%s does not export zn_module", entry.path.c_str());
        return;
    }
    if (m->target_api_version > ZYGISK_NEXT_API_VERSION) {
        LOGW("%s requires API version %d, only up to %d supported", entry.path.c_str(),
             m->target_api_version, ZYGISK_NEXT_API_VERSION);
        return;
    }

    auto* handle = new ModuleHandle();
    handle->lib_path = entry.path;

    if (entry.companion && m->target_api_version >= 3) {
        handle->companion_nonce = plan.nonce;
        handle->companion_listen_fd = openCompanionChannel(plan, entry);
    } else if (entry.companion) {
        LOGW("module %s declares companion but targets API %d (< 3), "
             "skipping companion process",
             entry.path.c_str(), m->target_api_version);
    }

    LOGI("loading module %s (companion=%s, api=%d)", entry.path.c_str(),
         entry.companion ? "yes" : "no", m->target_api_version);
    m->onModuleLoaded(handle, api::apiForVersion(m->target_api_version));
}

}  //namespace

void loadAll(const Plan& plan) {
    LOGI("loading %zu module(s) from the boot plan", plan.modules.size());
    for (const auto& entry : plan.modules) loadEntry(plan, entry);
}

}  //namespace znn::module
