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
#include "companion/process.h"
#include "ipc/client.h"
#include "log.h"
#include "module/entry.h"
#include "module/handle.h"
#include "process/self.h"
#include "utils/dlext.h"

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>
#include <vector>

namespace znn::module {
namespace {

constexpr char kModulesDir[] = "/data/adb/modules";

void loadEntry(const Entry& e, int module_fd = -1) {
    std::string lib_path;
    if (module_fd >= 0) {
        lib_path = e.lib;
    } else if (!resolveLibPath(e, lib_path)) {
        LOGE("module lib path %s is not inside module dir, skipping", e.lib.c_str());
        return;
    }

    void* lib = module_fd >= 0 ? dlopenFd(module_fd, lib_path.c_str(), RTLD_NOW)
                               : dlopenMemfd(lib_path.c_str(), RTLD_NOW);
    if (!lib) {
        LOGE("dlopen %s failed: %s", lib_path.c_str(), dlerror());
        return;
    }

    auto* m = reinterpret_cast<ZygiskNextModule*>(dlsym(lib, "zn_module"));
    if (!m || !m->onModuleLoaded) {
        LOGE("%s does not export zn_module", lib_path.c_str());
        return;
    }

    if (m->target_api_version > ZYGISK_NEXT_API_VERSION) {
        LOGW("%s requires API version %d, only up to %d supported", lib_path.c_str(),
             m->target_api_version, ZYGISK_NEXT_API_VERSION);
        return;
    }

    auto* handle = new ModuleHandle();
    handle->lib_path = lib_path;

    if (e.companion && m->target_api_version >= 3) {
        int cfd = ipc::spawnCompanion(lib_path);
        if (cfd >= 0) {
            LOGI("companion for %s spawned by injector daemon (fd %d)", lib_path.c_str(), cfd);
            handle->companion_fd = cfd;
            handle->companion_pid = -1;
        } else {
            int sv[2];
            if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) == 0) {
                pid_t pid = fork();
                if (pid == 0) {
                    close(sv[0]);
                    companion::run(lib_path.c_str(), sv[1]);
                    _exit(0);
                } else if (pid > 0) {
                    close(sv[1]);
                    handle->companion_fd = sv[0];
                    handle->companion_pid = pid;
                } else {
                    close(sv[0]);
                    close(sv[1]);
                }
            }
        }
    } else if (e.companion) {
        LOGW("module %s declares companion but targets API %d (< 3), "
             "skipping companion process",
             lib_path.c_str(), m->target_api_version);
    }

    LOGI("loading module %s (companion=%s, api=%d)", lib_path.c_str(), e.companion ? "yes" : "no",
         m->target_api_version);
    m->onModuleLoaded(handle, api::apiForVersion(m->target_api_version));
}

void loadFromFilesystem() {
    DIR* d = opendir(kModulesDir);
    if (!d) {
        LOGW("cannot open %s: %s", kModulesDir, strerror(errno));
        return;
    }

    std::vector<std::string> moddirs;
    struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        std::string dir = std::string(kModulesDir) + "/" + de->d_name;
        if (access((dir + "/disable").c_str(), F_OK) == 0) continue;
        if (access((dir + "/remove").c_str(), F_OK) == 0) continue;
        if (access((dir + "/zn_modules.txt").c_str(), R_OK) != 0) continue;
        moddirs.push_back(std::move(dir));
    }
    closedir(d);

    for (const auto& moddir : moddirs) {
        for (auto& e : parseManifest(moddir, moddir + "/zn_modules.txt")) {
            if (matches(e)) loadEntry(e);
        }
    }
}

}  //namespace

void loadAll() {
    std::vector<ipc::DaemonModule> mods;
    if (ipc::listModules(process::exeName(), process::exePath(), mods)) {
        LOGI("loadAllModules: got %zu module(s) from injector daemon", mods.size());
        for (auto& m : mods) {
            Entry e;
            e.is_name = true;
            e.target = process::exeName();
            e.companion = m.companion;
            e.lib = m.lib_path;
            loadEntry(e, m.fd);
            if (m.fd >= 0) close(m.fd);
        }
        return;
    }
    LOGW("loadAllModules: daemon unavailable, falling back to direct /data/adb/modules read");
    loadFromFilesystem();
}

}  //namespace znn::module
