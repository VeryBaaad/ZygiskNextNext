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

#include "utils/elf_util.h"
#include "include/zygisk_next_api.h"

#include <lsplt.hpp>

#include <dobby.h>

#include <android/log.h>
#include <android/dlext.h>
#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/memfd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <unistd.h>

#include <mutex>
#include <set>
#include <string>
#include <vector>

#define LOG_TAG "ZNNloader"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace znn;

// dlopen() a library from /data/adb fails for many target processes because the
// linker's namespace "permitted path" check rejects non-system paths. Loading
// from a file descriptor (ANDROID_DLEXT_USE_LIBRARY_FD) bypasses the path
// check, but modern bionic re-checks namespace accessibility for every fd that
// is not on tmpfs — so an fd for the real file under /data/adb is rejected
// too. Copy the bytes into a memfd (tmpfs) and dlopen from that instead.
//
// Reading/writing the memfd requires the target domain to access "tmpfs"
// (GKI) or "unlabeled" (other kernels) files — the ZNN module ships those
// sepolicy rules. The fd is deliberately not closed: bionic does not take
// ownership of USE_LIBRARY_FD fds and may keep reading from it.
static void* dlopenViaFd(const char* path, int flags) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        LOGE("dlopen %s: cannot open: %s", path, strerror(errno));
        return nullptr;
    }

    int memfd = static_cast<int>(syscall(SYS_memfd_create, "znn-module", MFD_CLOEXEC));
    if (memfd < 0) {
        LOGE("dlopen %s: memfd_create failed: %s", path, strerror(errno));
        close(fd);
        return nullptr;
    }

    char buf[16384];
    ssize_t n;
    bool ok = true;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        ssize_t left = n;
        const char* p = buf;
        while (left > 0) {
            ssize_t w = write(memfd, p, static_cast<size_t>(left));
            if (w <= 0) {
                ok = false;
                break;
            }
            p += w;
            left -= w;
        }
        if (!ok) break;
    }
    close(fd);
    if (!ok || n < 0) {
        LOGE("dlopen %s: copy to memfd failed: %s", path, strerror(errno));
        close(memfd);
        return nullptr;
    }

    android_dlextinfo extinfo = {};
    extinfo.flags = ANDROID_DLEXT_USE_LIBRARY_FD;
    extinfo.library_fd = memfd;

    void* lib = android_dlopen_ext(path, flags, &extinfo);
    if (!lib) LOGE("dlopen %s via memfd failed: %s", path, dlerror());
    // Keep the memfd open for the lifetime of the library (see comment above).
    return lib;
}

// Opaque symbol resolver handed out to modules.
struct ZnSymbolResolver {
    ElfImage* image = nullptr;
};

// Per-module handle; `connectCompanion` resolves the companion through it.
struct ModuleHandle {
    std::string lib_path;
    int companion_fd = -1;  // parent end of the control socketpair
    pid_t companion_pid = -1;
};

namespace {

constexpr char kCmdConnect = 1;

// Companion control channel to the injector daemon. The daemon runs as root,
// so a companion spawned by it inherits the daemon's privileged SELinux
// domain — required for modules whose companion process must talk to other
// privileged daemons (e.g. LSPosed's companion connects to the lspd daemon's
// "lspbridge" socket). ZygiskNextNext mirrors ZygiskNext here: the original ZN
// daemon spawns `zn-companion64` from itself, and LSPosed reports "Zygisk Next
// monitor ready" only when its ZN companion callbacks are delivered by such a
// privileged companion process.
constexpr char kCompanionSock[] = "/data/adb/zygisknextsu/companion.sock";
constexpr uint32_t kCompanionReqMagic = 0x5A4E4E43;  // "ZNNC"
// Commands sent over the companion socket (see injector.cpp handleCompanionRequest).
constexpr uint32_t kCompanionCmdSpawn = 1;    // spawn companion for lib_path
constexpr uint32_t kCompanionCmdModules = 2;  // list modules matching this process

std::mutex g_hook_mutex;
std::set<uintptr_t> g_hooked;  // addresses currently inline-hooked

// API implementation

int api_pltHook(void* base, const char* symbol, void* hook, void** original) {
    if (!base || !symbol || !hook) return ZN_FAILED;

    LOGI("pltHook base=%p symbol=%s", base, symbol);
    const uintptr_t b = reinterpret_cast<uintptr_t>(base);
    for (const auto& m : parseMaps("self")) {
        if (m.start != b || m.offset != 0) continue;

        LOGI("pltHook %s: dev=%llu inode=%llu path=%s", symbol,
             static_cast<unsigned long long>(m.dev), static_cast<unsigned long long>(m.inode),
             m.path.c_str());
        void* backup = nullptr;
        if (!lsplt::RegisterHook(m.dev, m.inode, symbol, hook, &backup)) {
            LOGE("pltHook %s: RegisterHook failed", symbol);
            return ZN_FAILED;
        }
        if (!lsplt::CommitHook()) {
            LOGE("pltHook %s: CommitHook failed", symbol);
            return ZN_FAILED;
        }
        if (original) *original = backup;
        return backup ? ZN_SUCCESS : ZN_FAILED;
    }
    LOGE("pltHook %s: base %p not found in maps", symbol, base);
    return ZN_FAILED;
}

int api_inlineHook(void* target, void* addr, void** original) {
    if (!target || !addr) return ZN_FAILED;

    const uintptr_t t = reinterpret_cast<uintptr_t>(target);
    {
        bool mapped = false;
        for (const auto& m : parseMaps("self")) {
            if (t >= m.start && t < m.end) {
                mapped = true;
                LOGI("inlineHook target %p -> %s+0x%lx", reinterpret_cast<void*>(t),
                     m.path.c_str(), static_cast<unsigned long>(t - m.start));
                break;
            }
        }
        if (!mapped) {
            LOGW("inlineHook target %p is not mapped", reinterpret_cast<void*>(t));
            // Diagnostic: dump the libc maps so a wrong base can be spotted.
            for (const auto& m : parseMaps("self")) {
                if (m.path.find("libc.so") == std::string::npos) continue;
                LOGI("  map %p-%p off=0x%lx %s", reinterpret_cast<void*>(m.start),
                     reinterpret_cast<void*>(m.end), static_cast<unsigned long>(m.offset),
                     m.path.c_str());
            }
        }

        std::lock_guard<std::mutex> lk(g_hook_mutex);
        // One address may be hooked only once, as required by the ZN contract.
        if (g_hooked.count(t)) return ZN_FAILED;
    }

    if (DobbyHook(target, reinterpret_cast<dobby_dummy_func_t>(addr),
                  reinterpret_cast<dobby_dummy_func_t*>(original)) != RS_SUCCESS) {
        LOGE("inlineHook %p: DobbyHook failed", reinterpret_cast<void*>(t));
        return ZN_FAILED;
    }

    std::lock_guard<std::mutex> lk(g_hook_mutex);
    g_hooked.insert(t);
    return ZN_SUCCESS;
}

int api_inlineUnhook(void* target) {
    if (!target) return ZN_FAILED;

    const uintptr_t t = reinterpret_cast<uintptr_t>(target);
    if (DobbyDestroy(target) != RS_SUCCESS) return ZN_FAILED;

    std::lock_guard<std::mutex> lk(g_hook_mutex);
    g_hooked.erase(t);
    return ZN_SUCCESS;
}

ZnSymbolResolver* api_newSymbolResolver(const char* path, void* base) {
    if (!path) return nullptr;

    auto* resolver = new ZnSymbolResolver();
    resolver->image = new ElfImage(path, reinterpret_cast<uintptr_t>(base));
    if (!resolver->image->valid()) {
        LOGW("newSymbolResolver %s: invalid", path);
        delete resolver->image;
        delete resolver;
        return nullptr;
    }
    LOGI("newSymbolResolver %s -> %s base=%p", path, resolver->image->path().c_str(),
         reinterpret_cast<void*>(resolver->image->base()));
    return resolver;
}

void api_freeSymbolResolver(ZnSymbolResolver* resolver) {
    if (!resolver) return;
    delete resolver->image;
    delete resolver;
}

void* api_getBaseAddress(ZnSymbolResolver* resolver) {
    if (!resolver) return nullptr;
    return reinterpret_cast<void*>(resolver->image->base());
}

void* api_symbolLookup(ZnSymbolResolver* resolver, const char* name, bool prefix, size_t* size) {
    if (!resolver || !name) return nullptr;

    const SymbolInfo* s = resolver->image->lookup(name, prefix);
    if (s && size) *size = s->size;

    if (!prefix) {
        // For exact lookups prefer the dynamic linker: it yields the true
        // runtime address of any loaded exported symbol, while the ELF-parsed
        // address can be wrong when the resolver's file image differs from the
        // mapped image.
        if (void* d = dlsym(RTLD_DEFAULT, name)) {
            LOGI("symbolLookup %s -> %p (dlsym)", name, d);
            return d;
        }
        // Not exported: resolve against the runtime in-memory dynsym (finds
        // hidden symbols the linker does not expose).
        uintptr_t a = resolver->image->runtimeLookup(name, size);
        if (a) {
            LOGI("symbolLookup %s -> %p (runtime)", name, reinterpret_cast<void*>(a));
            return reinterpret_cast<void*>(a);
        }
    }
    if (!s) return nullptr;
    LOGI("symbolLookup %s -> %p (elf)", name, reinterpret_cast<void*>(s->addr));
    return reinterpret_cast<void*>(s->addr);
}

void api_forEachSymbols(ZnSymbolResolver* resolver,
                        bool (*callback)(const char* name, void* addr, size_t size, void* data),
                        void* data) {
    if (!resolver || !callback) return;
    resolver->image->forEach([&](const char* name, uintptr_t addr, size_t size) {
        return callback(name, reinterpret_cast<void*>(addr), size, data);
    });
}

int api_connectCompanion(void* handle) {
    auto* h = static_cast<ModuleHandle*>(handle);
    if (!h || h->companion_fd < 0) return -1;

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) != 0) return -1;

    char cmd = kCmdConnect;
    struct iovec iov = {&cmd, sizeof(cmd)};
    char cmsg_buf[CMSG_SPACE(sizeof(int))] = {0};
    struct msghdr msg = {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &sv[1], sizeof(int));
    msg.msg_controllen = cmsg->cmsg_len;

    if (sendmsg(h->companion_fd, &msg, 0) < 0) {
        close(sv[0]);
        close(sv[1]);
        return -1;
    }
    close(sv[1]);
    return sv[0];
}

// ZN API v4 Runtime: the HyperOS Rust Runtime (HYOS), for apps spawned by
// /system_ext/bin/hyos_spawner. getRuntime() returns it only inside hyos_spawner;
// it is a plain static flag, so forked children inherit the runtime and the
// registered modules. onAppSpecialized fires after the child's SELinux app
// context is applied (detected by inline-hooking libselinux setcon/setexeccon);
// fork is observed via pthread_atfork.

constexpr int kMaxHyosModules = 4;

// HYOS runtime state. Plain statics so that forked children inherit the
// registered modules and the "in child / already fired" markers.
bool g_hyos_runtime = false;   // current process is hyos_spawner (or a child)
ZygiskNextHyosModule g_hyos_modules[kMaxHyosModules] = {};
int g_hyos_module_count = 0;
bool g_hyos_in_child = false;  // set by the atfork child handler
bool g_hyos_fired = false;     // onAppSpecialized already delivered here

// Saved originals of the hooked libselinux entry points.
int (*g_orig_setcon)(const char*) = nullptr;
int (*g_orig_setexeccon)(const char*) = nullptr;
bool g_hyos_warned = false;  // log the "hooks unavailable" warning only once

// Read the process name: /proc/self/cmdline first token, fallback PR_GET_NAME.
static void hyosReadProcessName(char* buf, size_t size) {
    buf[0] = '\0';
    int fd = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
    if (fd >= 0) {
        ssize_t n = read(fd, buf, size - 1);
        close(fd);
        if (n > 0) {
            // cmdline is NUL-separated; the first token is the process name.
            buf[n] = '\0';
            if (buf[0] != '\0') return;
        }
    }
    if (prctl(PR_GET_NAME, buf, 0, 0, 0) == 0 && buf[0] != '\0') return;
    strlcpy(buf, "hyos_app", size);
}

// Deliver onAppSpecialized to every registered module. Runs in the specialized
// child, right after the SELinux app context has been applied.
static void hyosDeliverAppSpecialized() {
    if (!g_hyos_in_child || g_hyos_fired || g_hyos_module_count == 0) return;
    g_hyos_fired = true;

    static char process_name[256];
    static char package_name[256];
    static char se_info[64];

    hyosReadProcessName(process_name, sizeof(process_name));

    // package_name: Android names a secondary process "pkg:proc"; the main
    // process is named after the package itself.
    strlcpy(package_name, process_name, sizeof(package_name));
    char* colon = strchr(package_name, ':');
    if (colon) *colon = '\0';

    // se_info is not exposed by hyos_spawner; check a few plausible env vars
    // (inherited by the child) and fall back to an empty, non-null string.
    se_info[0] = '\0';
    const char* env = getenv("HYOS_SEINFO");
    if (!env) env = getenv("SEINFO");
    if (env) strlcpy(se_info, env, sizeof(se_info));

    ZnHyosAppSpecializeArgs args = {process_name, package_name, se_info};
    LOGI("HYOS app specialized: process=%s package=%s se_info=%s", process_name,
         package_name, se_info);
    for (int i = 0; i < g_hyos_module_count; ++i) {
        if (g_hyos_modules[i].onAppSpecialized) {
            g_hyos_modules[i].onAppSpecialized(&args);
        }
    }
}

static int hyosSetconHook(const char* context) {
    int ret = g_orig_setcon ? g_orig_setcon(context) : -1;
    hyosDeliverAppSpecialized();
    return ret;
}

static int hyosSetexecconHook(const char* context) {
    int ret = g_orig_setexeccon ? g_orig_setexeccon(context) : -1;
    hyosDeliverAppSpecialized();
    return ret;
}

// Runs in every forked child, before the child's own post-fork code. Must not
// take locks or allocate (post-fork child of a possibly multithreaded process).
static void hyosAtForkChild() {
    g_hyos_in_child = true;
    g_hyos_fired = false;
}

// Defined below; forward-declared for the atfork prepare handler.
static void hyosInstallHooks();

// Runs in the parent right before it forks. Retries the libselinux hook
// installation: at registerModule time hyos_spawner's own libraries may not be
// loaded yet (the loader is injected before main), so the first attempt can
// legitimately fail; by the time the spawner forks an app child its libraries
// are in memory and the hooks can be installed.
static void hyosAtForkPrepare() {
    if (g_hyos_module_count > 0 && (!g_orig_setcon || !g_orig_setexeccon)) {
        hyosInstallHooks();
    }
}

// Inline-hook libselinux's setcon/setexeccon so that onAppSpecialized fires
// once the child applies its app context (the last specialization step).
// Failure is non-fatal: if the library is not loaded (or already hooked), the
// HYOS callback simply never fires.
static void hyosInstallHooks() {
    if (!g_orig_setcon) {
        void* setcon = dlsym(RTLD_DEFAULT, "setcon");
        if (setcon &&
            DobbyHook(setcon, reinterpret_cast<dobby_dummy_func_t>(hyosSetconHook),
                      reinterpret_cast<dobby_dummy_func_t*>(&g_orig_setcon)) == RS_SUCCESS) {
            LOGI("HYOS: hooked setcon");
        }
    }
    if (!g_orig_setexeccon) {
        void* setexeccon = dlsym(RTLD_DEFAULT, "setexeccon");
        if (setexeccon &&
            DobbyHook(setexeccon, reinterpret_cast<dobby_dummy_func_t>(hyosSetexecconHook),
                      reinterpret_cast<dobby_dummy_func_t*>(&g_orig_setexeccon)) == RS_SUCCESS) {
            LOGI("HYOS: hooked setexeccon");
        }
    }
    if (!g_orig_setcon && !g_orig_setexeccon && !g_hyos_warned) {
        g_hyos_warned = true;
        LOGW("HYOS: libselinux setcon/setexeccon not available, "
             "onAppSpecialized will not fire");
    }
}

int api_hyos_registerModule(const void* module) {
    if (!module) return ZN_FAILED;
    const auto* m = static_cast<const ZygiskNextHyosModule*>(module);
    if (!m->onAppSpecialized) return ZN_FAILED;
    if (m->target_api_version > ZYGISK_NEXT_HYOS_API_VERSION) {
        LOGE("HYOS: module targets API version %d, only up to %d supported",
             m->target_api_version, ZYGISK_NEXT_HYOS_API_VERSION);
        return ZN_FAILED;
    }
    if (g_hyos_module_count >= kMaxHyosModules) {
        LOGE("HYOS: too many modules registered (max %d)", kMaxHyosModules);
        return ZN_FAILED;
    }
    // The runtime copies the supplied structure before returning (per the API
    // contract), so later mutations by the module are not observed.
    g_hyos_modules[g_hyos_module_count++] = *m;

    if (!g_hyos_in_child) {
        // Register the fork handlers in the spawner; children inherit them.
        pthread_atfork(hyosAtForkPrepare, nullptr, hyosAtForkChild);
        hyosInstallHooks();
    }
    LOGI("HYOS: module registered (%d)", g_hyos_module_count);
    return ZN_SUCCESS;
}

static const ZygiskNextRuntime kHyosRuntime = {
    .type = ZN_RUNTIME_HYOS,
    .api_version = ZYGISK_NEXT_HYOS_API_VERSION,
    .registerModule = api_hyos_registerModule,
};

// Full implementation: exposed only to modules targeting API version >= 4.
static const ZygiskNextRuntime* api_getRuntime() {
    return g_hyos_runtime ? &kHyosRuntime : nullptr;
}

// Stub exposed to modules targeting API version < 4: the Runtime API was
// introduced in ZN API v4.
static const ZygiskNextRuntime* api_getRuntimeUnavailable() {
    LOGE("Runtime API requires module API version >= 4 (getRuntime)");
    return nullptr;
}

// The API is tiered by the module's `target_api_version`: hooks always; v2 adds
// the Symbol Resolver, v3 the Companion, v4 the Runtime (HYOS). Older modules
// get stub entries that fail like the documented cases instead of crashing.

// Stubs exposed to modules targeting API version < 2.
static struct ZnSymbolResolver* api_symbolResolverUnavailable(const char* path, void*) {
    LOGE("Symbol Resolver API requires module API version >= 2 (newSymbolResolver %s)",
         path ? path : "(null)");
    return nullptr;
}
static void api_freeSymbolResolverUnavailable(struct ZnSymbolResolver*) {
    LOGE("Symbol Resolver API requires module API version >= 2 (freeSymbolResolver)");
}
static void* api_getBaseAddressUnavailable(struct ZnSymbolResolver*) {
    LOGE("Symbol Resolver API requires module API version >= 2 (getBaseAddress)");
    return nullptr;
}
static void* api_symbolLookupUnavailable(struct ZnSymbolResolver*, const char* name, bool,
                                         size_t*) {
    LOGE("Symbol Resolver API requires module API version >= 2 (symbolLookup %s)",
         name ? name : "(null)");
    return nullptr;
}
static void api_forEachSymbolsUnavailable(struct ZnSymbolResolver*, bool (*)(const char*,
                                                                            void*, size_t,
                                                                            void*),
                                          void*) {
    LOGE("Symbol Resolver API requires module API version >= 2 (forEachSymbols)");
}

// Full API: for modules targeting API version >= 4 (incl. the Runtime API).
const ZygiskNextAPI kApiV4 = {
    api_pltHook,           api_inlineHook,   api_inlineUnhook,
    api_newSymbolResolver, api_freeSymbolResolver, api_getBaseAddress,
    api_symbolLookup,      api_forEachSymbols,
    api_connectCompanion,
    api_getRuntime,
};

// Full API: for modules targeting API version 2..3 (no Runtime API).
const ZygiskNextAPI kApiFull = {
    api_pltHook,           api_inlineHook,   api_inlineUnhook,
    api_newSymbolResolver, api_freeSymbolResolver, api_getBaseAddress,
    api_symbolLookup,      api_forEachSymbols,
    api_connectCompanion,
    api_getRuntimeUnavailable,
};

// Restricted API: for modules targeting API version < 2 (no Symbol Resolver).
const ZygiskNextAPI kApiNoSymbolResolver = {
    api_pltHook,                   api_inlineHook,             api_inlineUnhook,
    api_symbolResolverUnavailable, api_freeSymbolResolverUnavailable,
    api_getBaseAddressUnavailable, api_symbolLookupUnavailable,
    api_forEachSymbolsUnavailable,
    api_connectCompanion,
    api_getRuntimeUnavailable,
};

// Companion process

// Ask the injector daemon (running as root) to spawn the companion process for
// `lib_path` and hand back the control socket. The daemon-spawned companion
// runs in the daemon's own SELinux domain, which modules like LSPosed require
// (their companion must connect to privileged daemons such as the lspd
// "lspbridge" socket). Returns the control fd, or -1 when the daemon is not
// reachable — the caller then falls back to an in-process fork.
static int requestDaemonCompanion(const std::string& lib_path) {
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    strlcpy(addr.sun_path, kCompanionSock, sizeof(addr.sun_path));
    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }

    // Bound the wait for the daemon's reply so a stuck daemon cannot hang
    // module loading; the caller falls back to an in-process fork then.
    struct timeval tv {};
    tv.tv_sec = 5;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Request frame: magic u32 | cmd u32 | path length u32 (incl. NUL) | path bytes.
    const uint32_t magic = kCompanionReqMagic;
    const uint32_t cmd = kCompanionCmdSpawn;
    const uint32_t plen = static_cast<uint32_t>(lib_path.size() + 1);
    auto writeAll = [&](const void* buf, size_t len) {
        const auto* p = static_cast<const char*>(buf);
        size_t off = 0;
        while (off < len) {
            const ssize_t n = write(fd, p + off, len - off);
            if (n <= 0) return false;
            off += static_cast<size_t>(n);
        }
        return true;
    };
    if (!writeAll(&magic, sizeof(magic)) || !writeAll(&cmd, sizeof(cmd)) ||
        !writeAll(&plen, sizeof(plen)) || !writeAll(lib_path.c_str(), plen)) {
        close(fd);
        return -1;
    }

    // Receive the control socket (SCM_RIGHTS); the received byte is dropped
    // (the daemon may send an acknowledgement in the future).
    uint32_t ack = 0;
    struct iovec iov {&ack, sizeof(ack)};
    char cmsg_buf[CMSG_SPACE(sizeof(int))] = {0};
    struct msghdr msg {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    ssize_t n = recvmsg(fd, &msg, 0);
    int cfd = -1;
    if (n >= 0) {
        for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
                memcpy(&cfd, CMSG_DATA(cmsg), sizeof(int));
                break;
            }
        }
    }
    close(fd);
    return cfd;
}

// dlopen a library from a file descriptor (a memfd received from the injector
// daemon). bionic's namespace "permitted path" check is bypassed for a tmpfs
// fd, and — crucially — the loader does not need to open /data/adb/modules
// itself, which non-root targets (e.g. artd, uid 1082) cannot read. The fd is
// deliberately left open: bionic does not own USE_LIBRARY_FD fds.
static void* dlopenFromFd(int fd, const char* name, int flags) {
    android_dlextinfo ext = {};
    ext.flags = ANDROID_DLEXT_USE_LIBRARY_FD;
    ext.library_fd = fd;
    return android_dlopen_ext(name, flags, &ext);
}

// Read exactly `len` bytes from `fd` via recvmsg. When `out_fd` is non-null,
// capture the first SCM_RIGHTS fd delivered (a module record's memfd) and then
// stop capturing. Returns true on success.
static bool recvFull(int fd, void* buf, size_t len, int* out_fd) {
    auto* p = static_cast<char*>(buf);
    size_t off = 0;
    while (off < len) {
        char cmsg_buf[CMSG_SPACE(sizeof(int))] = {0};
        struct iovec iov {p + off, len - off};
        struct msghdr msg {};
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        if (out_fd) {
            msg.msg_control = cmsg_buf;
            msg.msg_controllen = sizeof(cmsg_buf);
        }
        ssize_t n = recvmsg(fd, &msg, 0);
        if (n <= 0) return false;
        if (out_fd) {
            for (struct cmsghdr* c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
                if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS) {
                    memcpy(out_fd, CMSG_DATA(c), sizeof(int));
                    break;
                }
            }
            out_fd = nullptr;
        }
        off += static_cast<size_t>(n);
    }
    return true;
}

// One module the daemon resolved for the current process. `fd` is a memfd
// holding the module library, handed over by the root injector daemon so the
// loader never has to read /data/adb/modules itself.
struct DaemonModule {
    std::string lib_path;
    bool companion = false;
    int fd = -1;
};

// Ask the injector daemon (root) for the modules that apply to the current
// process. The daemon reads every module's zn_modules.txt, matches against
// `process_name`/`process_path`, memfd's each matching library and streams the
// records (lib path, companion flag, memfd fd) back over SCM_RIGHTS. Returns
// true on success (empty vector is a valid result).
static bool requestModulesFromDaemon(const std::string& process_name,
                                     const std::string& process_path,
                                     std::vector<DaemonModule>& out) {
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return false;

    struct sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    strlcpy(addr.sun_path, kCompanionSock, sizeof(addr.sun_path));
    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return false;
    }
    struct timeval tv {};
    tv.tv_sec = 5;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    const uint32_t magic = kCompanionReqMagic;
    const uint32_t cmd = kCompanionCmdModules;
    const uint32_t nlen = static_cast<uint32_t>(process_name.size() + 1);
    const uint32_t plen = static_cast<uint32_t>(process_path.size() + 1);
    auto writeAll = [&](const void* buf, size_t len) {
        const auto* p = static_cast<const char*>(buf);
        size_t off = 0;
        while (off < len) {
            const ssize_t n = write(fd, p + off, len - off);
            if (n <= 0) return false;
            off += static_cast<size_t>(n);
        }
        return true;
    };
    if (!writeAll(&magic, sizeof(magic)) || !writeAll(&cmd, sizeof(cmd)) ||
        !writeAll(&nlen, sizeof(nlen)) || !writeAll(process_name.c_str(), nlen) ||
        !writeAll(&plen, sizeof(plen)) || !writeAll(process_path.c_str(), plen)) {
        close(fd);
        return false;
    }

    for (;;) {
        uint32_t rlen = 0;
        int mfd = -1;
        if (!recvFull(fd, &rlen, sizeof(rlen), &mfd)) {
            close(fd);
            return false;
        }
        if (rlen == 0) break;  // terminator
        if (rlen > 4096) {
            close(fd);
            return false;
        }
        std::string lib(rlen, '\0');
        uint32_t comp = 0;
        if (!recvFull(fd, &lib[0], rlen, nullptr) ||
            !recvFull(fd, &comp, sizeof(comp), nullptr) || lib[rlen - 1] != '\0' || mfd < 0) {
            close(fd);
            return false;
        }
        lib.resize(rlen - 1);
        out.push_back({std::move(lib), comp != 0, mfd});
    }
    close(fd);
    return true;
}

[[noreturn]] void companionMain(const char* lib_path, int ctl_fd) {
    void* lib = dlopenViaFd(lib_path, RTLD_NOW);
    if (!lib) {
        LOGE("companion: dlopen %s failed: %s", lib_path, dlerror());
        _exit(1);
    }

    auto* m = reinterpret_cast<ZygiskNextCompanionModule*>(dlsym(lib, "zn_companion_module"));
    if (!m || !m->onCompanionLoaded || !m->onModuleConnected) {
        LOGE("companion: %s does not export zn_companion_module", lib_path);
        _exit(1);
    }

    m->onCompanionLoaded();

    for (;;) {
        char cmd = 0;
        int fd = -1;
        struct iovec iov = {&cmd, sizeof(cmd)};
        char cmsg_buf[CMSG_SPACE(sizeof(int))] = {0};
        struct msghdr msg = {};
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = cmsg_buf;
        msg.msg_controllen = sizeof(cmsg_buf);

        ssize_t n = recvmsg(ctl_fd, &msg, 0);
        if (n <= 0) break;

        if (cmd != kCmdConnect) continue;

        for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
                memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));
                break;
            }
        }
        if (fd >= 0) {
            m->onModuleConnected(fd);  // module owns and closes fd
            fd = -1;
        }
    }
    _exit(0);
}

// Process identity

std::string getProcessPath() {
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return {};
    buf[n] = '\0';
    return buf;
}

std::string getProcessName() {
    const std::string p = getProcessPath();
    const auto pos = p.rfind('/');
    return pos == std::string::npos ? p : p.substr(pos + 1);
}

// zn_modules.txt parsing

struct ModuleEntry {
    bool is_name = false;   // name=... (true) or path=... (false)
    std::string target;     // the value to match against
    bool companion = false;
    std::string lib;        // relative (to module dir) or absolute lib path
    std::string module_dir;
};

std::vector<ModuleEntry> parseZnModulesFile(const std::string& moddir, const std::string& file) {
    std::vector<ModuleEntry> out;
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

        ModuleEntry e;
        e.module_dir = moddir;

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

bool matchEntry(const ModuleEntry& e) {
    if (e.is_name) return getProcessName() == e.target;
    return getProcessPath() == e.target;
}

// Resolve the module library to a real path that must reside inside the module dir.
bool resolveLibPath(const ModuleEntry& e, std::string& out) {
    std::string candidate = e.lib;
    if (candidate.empty() || candidate[0] != '/') candidate = e.module_dir + "/" + candidate;

    char real_mod[PATH_MAX];
    char real_lib[PATH_MAX];
    if (!realpath(e.module_dir.c_str(), real_mod)) return false;
    if (!realpath(candidate.c_str(), real_lib)) return false;

    const std::string mod = real_mod;
    const std::string lib = real_lib;
    if (lib.size() <= mod.size() || lib.compare(0, mod.size(), mod) != 0 || lib[mod.size()] != '/') {
        return false;
    }
    out = lib;
    return true;
}

void loadEntry(const ModuleEntry& e, int module_fd = -1) {
    std::string lib_path;
    if (module_fd >= 0) {
        // Provided by the root daemon: lib_path is already the resolved path,
        // and the module library is handed over as a memfd so the loader does
        // not have to read /data/adb/modules (non-root targets cannot).
        lib_path = e.lib;
    } else if (!resolveLibPath(e, lib_path)) {
        LOGE("module lib path %s is not inside module dir, skipping", e.lib.c_str());
        return;
    }

    void* lib = module_fd >= 0 ? dlopenFromFd(module_fd, lib_path.c_str(), RTLD_NOW)
                               : dlopenViaFd(lib_path.c_str(), RTLD_NOW);
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

    // The companion process is only started when the module targets API
    // version >= 3: the Companion API (connectCompanion) was introduced in ZN
    // API v3, so a module built against an older API cannot use it and forking
    // the companion would only waste a process. This gate is independent of the
    // `companion` flag in zn_modules.txt, which merely requests the companion.
    if (e.companion && m->target_api_version >= 3) {
        // Preferred: let the injector daemon (root) spawn the companion, so it
        // runs in the daemon's privileged SELinux domain. Modules such as
        // LSPosed need this: their companion connects to privileged daemons
        // (the lspd "lspbridge" socket) and only then can they announce the
        // "Zygisk Next monitor" to the LSPosed daemon.
        int cfd = requestDaemonCompanion(lib_path);
        if (cfd >= 0) {
            LOGI("companion for %s spawned by injector daemon (fd %d)", lib_path.c_str(), cfd);
            handle->companion_fd = cfd;
            handle->companion_pid = -1;
        } else {
            // Fallback: fork the companion in-process. This keeps companion
            // support working when the daemon is unreachable (early boot
            // races), at the cost of the target's SELinux domain — which is
            // fine for ordinary modules, but insufficient for privileged
            // companions (LSPosed).
            int sv[2];
            if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) == 0) {
                pid_t pid = fork();
                if (pid == 0) {
                    close(sv[0]);
                    companionMain(lib_path.c_str(), sv[1]);
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
             "skipping companion process", lib_path.c_str(), m->target_api_version);
    }

    // The Symbol Resolver API is only exposed to modules targeting API
    // version >= 2; lower-version modules get the restricted struct whose
    // resolver slots are failing stubs. The Companion API is gated separately
    // above (companion process only for target_api_version >= 3), and the
    // Runtime API (getRuntime / HYOS) is only exposed to modules targeting API
    // version >= 4.
    const ZygiskNextAPI* api;
    if (m->target_api_version >= 4) {
        api = &kApiV4;
    } else if (m->target_api_version >= 2) {
        api = &kApiFull;
    } else {
        api = &kApiNoSymbolResolver;
    }

    LOGI("loading module %s (companion=%s, api=%d)", lib_path.c_str(),
         e.companion ? "yes" : "no", m->target_api_version);
    m->onModuleLoaded(handle, api);
}

void loadAllModules() {
    // Preferred path: get the modules for this process from the root injector
    // daemon. The daemon reads zn_modules.txt and memfd's each matching library
    // as root, so the loader does not need to read /data/adb/modules — which
    // non-root targets (artd, uid 1082) cannot due to DAC permissions. This
    // also keeps module files unreadable to ordinary apps (no detection
    // surface), unlike making the module tree world-readable.
    std::vector<DaemonModule> mods;
    if (requestModulesFromDaemon(getProcessName(), getProcessPath(), mods)) {
        LOGI("loadAllModules: got %zu module(s) from injector daemon", mods.size());
        for (auto& m : mods) {
            ModuleEntry e;
            e.is_name = true;
            e.target = getProcessName();
            e.companion = m.companion;
            e.lib = m.lib_path;
            e.module_dir.clear();
            loadEntry(e, m.fd);
            if (m.fd >= 0) close(m.fd);
        }
        return;
    }
    LOGW("loadAllModules: daemon unavailable, falling back to direct /data/adb/modules read");

    // Fallback for root targets (netd, adbd): read /data/adb/modules directly.
    DIR* d = opendir("/data/adb/modules");
    if (!d) {
        LOGW("cannot open /data/adb/modules: %s", strerror(errno));
        return;
    }

    std::vector<std::string> moddirs;
    struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        std::string dir = std::string("/data/adb/modules/") + de->d_name;
        if (access((dir + "/disable").c_str(), F_OK) == 0) continue;
        if (access((dir + "/remove").c_str(), F_OK) == 0) continue;
        if (access((dir + "/zn_modules.txt").c_str(), R_OK) != 0) continue;
        moddirs.push_back(std::move(dir));
    }
    closedir(d);

    for (const auto& moddir : moddirs) {
        for (auto& e : parseZnModulesFile(moddir, moddir + "/zn_modules.txt")) {
            if (matchEntry(e)) loadEntry(e);
        }
    }
}

}  // namespace

// The injector calls this explicitly after android_dlopen_ext returns.
//
// Module loading must NOT happen from a constructor: the static constructors
// of this library and of the statically linked LSPlt/Dobby/elf_util code are
// ordered relative to a `__attribute__((constructor))` function by the
// linker's .init_array layout, and with -flto that order is not the
// declaration order. Calling loadAllModules() too early runs on
// not-yet-constructed statics (the LSPlt hook registry list head is NULL) and
// crashes inside the first pltHook. Waiting until dlopen has returned
// guarantees every static constructor has finished.
extern "C" __attribute__((visibility("default"))) void znn_loader_init() {
    LOGI("loader initialized in pid %d (%s)", getpid(), getProcessPath().c_str());

    // Expose the HyperOS Rust Runtime API when we are inside hyos_spawner
    // itself (the loader is injected there by matching
    // `path=/system_ext/bin/hyos_spawner` in zn_modules.txt). Children forked
    // from the spawner inherit this flag — and the registered HYOS modules —
    // through fork memory, so getRuntime() keeps working in the app processes.
    if (getProcessName() == "hyos_spawner") g_hyos_runtime = true;

    loadAllModules();
}
