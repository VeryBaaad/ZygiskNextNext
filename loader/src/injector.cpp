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

#include <android/log.h>
#include <ctype.h>
#include <dirent.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/xattr.h>
#include <sys/system_properties.h>
#include <time.h>
#include <unistd.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#ifndef PTRACE_GETREGSET
#define PTRACE_GETREGSET 0x4204
#endif
#ifndef PTRACE_SETREGSET
#define PTRACE_SETREGSET 0x4205
#endif
#ifndef NT_PRSTATUS
#define NT_PRSTATUS 1
#endif

// bionic's <linux/ptrace.h> does not expose the arch-generic register
// read/write requests; the values are stable across all Linux architectures.
#ifndef PTRACE_GETREGS
#define PTRACE_GETREGS 12
#endif
#ifndef PTRACE_SETREGS
#define PTRACE_SETREGS 13
#endif

#define LOG_TAG "ZNNinjector"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace znn;

namespace {

// Targets

struct Target {
    bool is_name = false;
    std::string value;
};

// One enabled ZN module declaration, plus the runtime results the WebUI
// reports. The definition part (id/name/version/targets) is rebuilt on every
// rescan; the runtime results (processes/failed) survive rescans because they
// are carried over by module id.
struct ModuleInfo {
    std::string id;
    std::string name;
    std::string version;
    std::vector<Target> targets;

    // Successfully injected processes (pid, process name). The count is the
    // number of real injections; entries are never pruned (a dead process is
    // simply left in the list).
    std::vector<std::pair<int, std::string>> processes;
    // Injection failures / misses (process name, short reason).
    std::vector<std::pair<std::string, std::string>> failed;
};

// Runtime directory, following the original Zygisk Next convention. The state
// snapshot the WebUI reads lives here; anything under /data/adb/zygisksu is
// fair game for the daemon (the module dir itself is root-managed and must
// stay untouched).
constexpr const char* kStateDir = "/data/adb/zygisksu";
constexpr const char* kStateFile = "/data/adb/zygisksu/znn_state.json";
constexpr const char* kStateTmp = "/data/adb/zygisksu/znn_state.json.tmp";

// Root implementation versions, collected once at startup.
struct RootImplInfo {
    std::string kernel_su;
    std::string magisk;
    std::string apatch;
};

struct SystemInfo {
    std::string kernel;
    int sdk = 0;
    std::string abi;
    std::string abilist;
    RootImplInfo root;
};

std::vector<Target> g_targets;
std::map<std::string, ModuleInfo> g_modules;  // module id -> definition + results
SystemInfo g_system;
std::string g_loader64;  // 64-bit libloader.so
std::string g_loader32;  // 32-bit libloader.so
std::string g_module_dir;

// State snapshot write bookkeeping: the snapshot is (re)written on demand or
// on the periodic rescan, throttled so a burst of injection events does not
// spam the disk.
bool g_state_dirty = false;

// 0 = classic (trace init), 1 = compat (poll /proc). Chosen automatically in
// main(): classic unless a Zygisk implementation is running or init is already
// held by another tracer; classic also yields to a Zygisk that starts later.
int g_mode = 0;

volatile sig_atomic_t g_rescan = 0;
void on_sighup(int) { g_rescan = 1; }

void collectTargets();  // defined below (uses readPropValue)

std::string readPropValue(const std::string& moddir, const char* key);

void collectTargets() {
    std::map<std::string, ModuleInfo> next;

    DIR* d = opendir("/data/adb/modules");
    if (!d) return;
    struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        std::string dir = std::string("/data/adb/modules/") + de->d_name;
        if (access((dir + "/disable").c_str(), F_OK) == 0) continue;
        if (access((dir + "/remove").c_str(), F_OK) == 0) continue;

        std::string file = dir + "/zn_modules.txt";
        FILE* f = fopen(file.c_str(), "re");
        if (!f) continue;

        ModuleInfo mi;
        mi.id = de->d_name;
        mi.name = readPropValue(dir, "name");
        if (mi.name.empty()) mi.name = mi.id;
        mi.version = readPropValue(dir, "version");

        char* line = nullptr;
        size_t cap = 0;
        while (getline(&line, &cap, f) > 0) {
            std::string l = line;
            std::vector<std::string> toks;
            size_t i = 0;
            while (i < l.size()) {
                while (i < l.size() && isspace((unsigned char)l[i])) ++i;
                size_t j = i;
                while (j < l.size() && !isspace((unsigned char)l[j])) ++j;
                if (j > i) toks.push_back(l.substr(i, j - i));
                i = j;
            }
            if (toks.size() < 2) continue;

            Target t;
            if (toks[0].rfind("path=", 0) == 0) {
                t.is_name = false;
                t.value = toks[0].substr(5);
            } else if (toks[0].rfind("name=", 0) == 0) {
                t.is_name = true;
                t.value = toks[0].substr(5);
            } else {
                continue;
            }
            mi.targets.push_back(std::move(t));
        }
        free(line);
        fclose(f);

        if (mi.targets.empty()) continue;

        // Carry the runtime results of the previous snapshot of this module
        // over to the rebuilt definition (they are keyed by module id).
        auto prev = g_modules.find(mi.id);
        if (prev != g_modules.end()) {
            mi.processes = std::move(prev->second.processes);
            mi.failed = std::move(prev->second.failed);
        }
        next[mi.id] = std::move(mi);
    }
    closedir(d);
    g_modules = std::move(next);

    // Flat target list for injection matching (deduplicated).
    std::set<std::pair<bool, std::string>> seen;
    std::vector<Target> targets;
    for (const auto& kv : g_modules) {
        for (const auto& t : kv.second.targets) {
            if (seen.insert({t.is_name, t.value}).second) targets.push_back(t);
        }
    }
    g_targets = std::move(targets);
}

// Read the first `key=value` line from a module's module.prop.
std::string readPropValue(const std::string& moddir, const char* key) {
    std::string file = moddir + "/module.prop";
    FILE* f = fopen(file.c_str(), "re");
    if (!f) return {};
    char* line = nullptr;
    size_t cap = 0;
    std::string out;
    const size_t klen = strlen(key);
    while (getline(&line, &cap, f) > 0) {
        std::string l = line;
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        if (l.size() > klen && l.rfind(key, 0) == 0 && l[klen] == '=') {
            out = l.substr(klen + 1);
            break;
        }
    }
    free(line);
    fclose(f);
    return out;
}

// WebUI state snapshot
// ====================

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// Best-effort display name of a process: /proc/<pid>/cmdline first token,
// falling back to the executable basename.
std::string procName(pid_t pid, const std::string& exe) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd >= 0) {
        char buf[512];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            std::string s(buf);
            while (!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
            while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
            if (!s.empty()) return s;
        }
    }
    auto pos = exe.rfind('/');
    return pos == std::string::npos ? exe : exe.substr(pos + 1);
}

// Does module `m` declare a target matching the process's executable?
bool moduleMatchesExe(const ModuleInfo& m, const std::string& exe) {
    if (exe.empty()) return false;
    const auto pos = exe.rfind('/');
    const std::string name = pos == std::string::npos ? exe : exe.substr(pos + 1);
    for (const auto& t : m.targets) {
        if (t.is_name ? (name == t.value) : (exe == t.value)) return true;
    }
    return false;
}

// Record a successful injection for every module that targets this process.
// Called from the STATE_INIT completion path, before detaching.
void recordSuccess(pid_t pid, const std::string& exe) {
    const std::string name = procName(pid, exe);
    for (auto& kv : g_modules) {
        if (moduleMatchesExe(kv.second, exe)) kv.second.processes.emplace_back(pid, name);
    }
    g_state_dirty = true;
}

// Record an injection failure/miss for every module that targets this process.
void recordFailure(pid_t pid, const std::string& exe, const std::string& reason) {
    const std::string name = procName(pid, exe);
    for (auto& kv : g_modules) {
        if (moduleMatchesExe(kv.second, exe)) kv.second.failed.emplace_back(name, reason);
    }
    g_state_dirty = true;
}

// Collect the (static) system info once at startup. This mirrors what the old
// znn_api.sh gathered on every WebUI request; doing it here means the WebUI
// reads a snapshot instead of spawning shells and probing the device.
void collectSystemInfo() {
    struct utsname u {};
    if (uname(&u) == 0) g_system.kernel = u.release;

    char buf[PROP_VALUE_MAX];
    if (__system_property_get("ro.build.version.sdk", buf) > 0) {
        g_system.sdk = atoi(buf);
    }
    if (__system_property_get("ro.product.cpu.abi", buf) > 0) g_system.abi = buf;
    if (__system_property_get("ro.product.cpu.abilist", buf) > 0) g_system.abilist = buf;

    auto firstLine = [](const char* bin, const char* arg) -> std::string {
        std::string cmd = bin;
        if (arg) cmd += std::string(" ") + arg;
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) return {};
        char line[256];
        std::string out;
        if (fgets(line, sizeof(line), p)) out = line;
        pclose(p);
        while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
        return out;
    };

    if (access("/data/adb/ksud", X_OK) == 0) {
        g_system.root.kernel_su = firstLine("/data/adb/ksud", "--version");
    }
    if (access("/data/adb/magisk/magisk", X_OK) == 0) {
        g_system.root.magisk = firstLine("/data/adb/magisk/magisk", "-v");
        if (g_system.root.magisk.empty()) g_system.root.magisk = firstLine("/data/adb/magisk/magisk", "-V");
    }
    if (access("/data/adb/apd", X_OK) == 0) {
        g_system.root.apatch = firstLine("/data/adb/apd", "--version");
    }
}

void appendRootJson(std::string& out, const RootImplInfo& r) {
    out += "{\"kernelSU\":\"" + jsonEscape(r.kernel_su) + "\",\"magisk\":\"" +
           jsonEscape(r.magisk) + "\",\"apatch\":\"" + jsonEscape(r.apatch) + "\"}";
}

void appendModuleJson(std::string& out, const ModuleInfo& m) {
    out += "{\"id\":\"" + jsonEscape(m.id) + "\",\"name\":\"" + jsonEscape(m.name) +
           "\",\"version\":\"" + jsonEscape(m.version) + "\",\"processes\":[";
    bool first = true;
    for (const auto& [pid, name] : m.processes) {
        if (!first) out += ",";
        first = false;
        out += "{\"pid\":" + std::to_string(pid) + ",\"name\":\"" + jsonEscape(name) + "\"}";
    }
    out += "],\"failed\":[";
    first = true;
    for (const auto& [name, reason] : m.failed) {
        if (!first) out += ",";
        first = false;
        out += "{\"name\":\"" + jsonEscape(name) + "\",\"reason\":\"" + jsonEscape(reason) + "\"}";
    }
    out += "]}";
}

std::string buildStateJson() {
    std::string out;
    out.reserve(2048);
    out += "{\"running\":true,\"pid\":" + std::to_string(getpid()) +
           ",\"mode\":\"" + (g_mode == 1 ? "compat" : "classic") +
           "\",\"zygiskCompat\":" + (g_mode == 1 ? "true" : "false") +
           ",\"version\":\"" + jsonEscape(ZNN_VERSION) + "\",\"system\":{";
    out += "\"kernel\":\"" + jsonEscape(g_system.kernel) + "\",\"sdk\":" + std::to_string(g_system.sdk) +
           ",\"abi\":\"" + jsonEscape(g_system.abi) + "\",\"abilist\":\"" + jsonEscape(g_system.abilist) +
           "\",\"root\":";
    appendRootJson(out, g_system.root);
    out += "},\"modules\":[";
    bool first = true;
    for (const auto& kv : g_modules) {
        if (!first) out += ",";
        first = false;
        appendModuleJson(out, kv.second);
    }
    out += "]}";
    return out;
}

// Atomically persist the snapshot (tmp file + rename), so a reader never
// observes a torn write. The daemon runs as root; /data/adb/zygisksu follows
// the original Zygisk Next runtime-directory convention.
void writeStateSnapshot() {
    mkdir(kStateDir, 0755);
    const std::string json = buildStateJson();

    int fd = open(kStateTmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) return;
    size_t off = 0;
    while (off < json.size()) {
        ssize_t n = write(fd, json.data() + off, json.size() - off);
        if (n <= 0) break;
        off += static_cast<size_t>(n);
    }
    fsync(fd);
    close(fd);
    if (off != json.size()) {
        unlink(kStateTmp);
        return;
    }
    rename(kStateTmp, kStateFile);
}

// Extract a balanced {…} or […] JSON value located after `marker`.
bool extractBalanced(const std::string& s, const char* marker, std::string* out) {
    size_t i = s.find(marker);
    if (i == std::string::npos) return false;
    i += strlen(marker);
    while (i < s.size() && isspace((unsigned char)s[i])) ++i;
    if (i >= s.size() || (s[i] != '{' && s[i] != '[')) return false;
    const char open = s[i], close = open == '{' ? '}' : ']';
    int depth = 0;
    bool in_str = false;
    for (size_t j = i; j < s.size(); ++j) {
        const char c = s[j];
        if (in_str) {
            if (c == '\\') { ++j; continue; }
            if (c == '"') in_str = false;
            continue;
        }
        if (c == '"') { in_str = true; continue; }
        if (c == open) ++depth;
        else if (c == close && --depth == 0) {
            *out = s.substr(i, j - i + 1);
            return true;
        }
    }
    return false;
}

// Look up `"key":value` (a scalar, no surrounding whitespace assumptions) and
// return the value verbatim.
bool extractScalar(const std::string& s, const char* key, std::string* out) {
    std::string pat = std::string("\"") + key + "\":";
    size_t i = s.find(pat);
    if (i == std::string::npos) return false;
    i += pat.size();
    size_t j = i;
    while (j < s.size() && s[j] != ',' && s[j] != '}') ++j;
    *out = s.substr(i, j - i);
    return true;
}

// Is `pid` an alive injector daemon?
bool isInjectorAlive(pid_t pid) {
    if (pid <= 1) return false;
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    char comm[64] = {0};
    const ssize_t n = read(fd, comm, sizeof(comm) - 1);
    close(fd);
    return n > 0 && strncmp(comm, "injector", 8) == 0;
}

// Fallback: find the daemon pid by scanning /proc comm (used when the state
// file is missing or stale, e.g. right after a daemon restart).
pid_t findInjectorPid() {
    DIR* d = opendir("/proc");
    if (!d) return 0;
    pid_t found = 0;
    struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] < '0' || de->d_name[0] > '9') continue;
        const pid_t pid = static_cast<pid_t>(strtol(de->d_name, nullptr, 10));
        if (pid > 1 && isInjectorAlive(pid)) {
            found = pid;
            break;
        }
    }
    closedir(d);
    return found;
}

// WebUI control interface: `injector --ctl <status|system|modules|rescan>`.
// Reads the daemon-written snapshot and prints JSON on stdout. This replaces
// the old znn_api.sh entirely: nothing is scanned on demand, the daemon's
// recorded state is simply echoed back.
int ctlMain(const char* cmd) {
    std::string state;
    {
        int fd = open(kStateFile, O_RDONLY | O_CLOEXEC);
        if (fd >= 0) {
            char buf[8192];
            ssize_t n;
            while ((n = read(fd, buf, sizeof(buf))) > 0) state.append(buf, static_cast<size_t>(n));
            close(fd);
        }
    }

    if (strcmp(cmd, "rescan") == 0) {
        pid_t pid = findInjectorPid();
        if (pid <= 0) {
            printf("injector is not running\n");
            return 1;
        }
        kill(pid, SIGHUP);
        printf("injector (%d) requested to rescan modules\n", pid);
        return 0;
    }

    if (strcmp(cmd, "status") == 0) {
        pid_t pid = 0;
        std::string spid, zmode;
        if (extractScalar(state, "pid", &spid)) pid = static_cast<pid_t>(strtol(spid.c_str(), nullptr, 10));
        extractScalar(state, "mode", &zmode);
        const bool alive = isInjectorAlive(pid);
        // The snapshot can be stale (daemon restarted); fall back to /proc.
        if (!alive) {
            pid_t found = findInjectorPid();
            if (found > 0) {
                pid = found;
            }
        }
        const bool running = pid > 0 && isInjectorAlive(pid);
        const bool compat = running && zmode.find("compat") != std::string::npos;
        printf("{\"running\":%s,\"pid\":%d,\"zygiskCompat\":%s,\"mode\":\"%s\"}\n",
               running ? "true" : "false", running ? pid : 0, compat ? "true" : "false",
               zmode.empty() ? "unknown" : zmode.c_str());
        return 0;
    }

    if (strcmp(cmd, "system") == 0) {
        std::string sys;
        if (extractBalanced(state, "\"system\":", &sys)) {
            printf("%s\n", sys.c_str());
        } else {
            printf("{\"kernel\":\"\",\"sdk\":0,\"abi\":\"\",\"abilist\":\"\",\"root\":"
                   "{\"kernelSU\":\"\",\"magisk\":\"\",\"apatch\":\"\"}}\n");
        }
        return 0;
    }

    if (strcmp(cmd, "modules") == 0) {
        std::string mods;
        if (extractBalanced(state, "\"modules\":", &mods)) {
            printf("%s\n", mods.c_str());
        } else {
            printf("[]\n");
        }
        return 0;
    }

    fprintf(stderr, "usage: injector --ctl <status|system|modules|rescan>\n");
    return 1;
}

// Ptrace memory helpers

bool readMem(pid_t pid, uintptr_t addr, void* buf, size_t len) {
    auto* p = static_cast<uint8_t*>(buf);
    while (len > 0) {
        uintptr_t aligned = addr & ~(sizeof(uintptr_t) - 1);
        size_t off = addr - aligned;
        errno = 0;
        long v = ptrace(PTRACE_PEEKDATA, pid, reinterpret_cast<void*>(aligned), nullptr);
        if (v == -1 && errno != 0) return false;
        uint8_t word[sizeof(uintptr_t)];
        memcpy(word, &v, sizeof(uintptr_t));
        size_t n = (len < sizeof(uintptr_t) - off) ? len : sizeof(uintptr_t) - off;
        memcpy(p, word + off, n);
        p += n;
        addr += n;
        len -= n;
    }
    return true;
}

bool writeMem(pid_t pid, uintptr_t addr, const void* buf, size_t len) {
    const auto* p = static_cast<const uint8_t*>(buf);
    while (len > 0) {
        uintptr_t aligned = addr & ~(sizeof(uintptr_t) - 1);
        size_t off = addr - aligned;
        uintptr_t word = 0;
        if (off != 0 || len < sizeof(uintptr_t)) {
            errno = 0;
            long v = ptrace(PTRACE_PEEKDATA, pid, reinterpret_cast<void*>(aligned), nullptr);
            if (v == -1 && errno != 0) return false;
            word = static_cast<uintptr_t>(v);
        }
        uint8_t bytes[sizeof(uintptr_t)];
        memcpy(bytes, &word, sizeof(uintptr_t));
        size_t n = (len < sizeof(uintptr_t) - off) ? len : sizeof(uintptr_t) - off;
        memcpy(bytes + off, p, n);
        memcpy(&word, bytes, sizeof(uintptr_t));
        errno = 0;
        if (ptrace(PTRACE_POKEDATA, pid, reinterpret_cast<void*>(aligned),
                   reinterpret_cast<void*>(word)) == -1 &&
            errno != 0) {
            return false;
        }
        p += n;
        addr += n;
        len -= n;
    }
    return true;
}

// Register layouts (all four ABIs, selected at runtime from the target ELF)

enum class Arch { kArm32, kArm64, kX86, kX86_64, kUnknown };

struct Arm64Regs {
    uint64_t regs[31];
    uint64_t sp, pc, pstate;
};
struct X64Regs {
    uint64_t r15, r14, r13, r12, rbp, rbx, r11, r10;
    uint64_t r9, r8, rax, rcx, rdx, rsi, rdi, orig_rax;
    uint64_t rip, cs, eflags, rsp, ss;
    uint64_t fs_base, gs_base, ds, es, fs, gs;
};
struct Arm32Regs {
    uint32_t uregs[18];  // r0..r15, cpsr, orig_r0
};
struct X86Regs {
    uint32_t ebx, ecx, edx, esi, edi, ebp, eax;
    uint32_t xds, xes, xfs, xgs;
    uint32_t orig_eax, eip, xcs, eflags, esp, xss;
};

struct Regs {
    Arch arch = Arch::kUnknown;
    union {
        Arm64Regs a64;
        X64Regs x64;
        Arm32Regs a32;
        X86Regs x86;
    } u;
};

bool getRegs(pid_t pid, Regs* r) {
    struct iovec io;
    io.iov_base = &r->u;
    switch (r->arch) {
        case Arch::kArm64:  io.iov_len = sizeof(Arm64Regs); break;
        case Arch::kX86_64: io.iov_len = sizeof(X64Regs);   break;
        case Arch::kArm32:  io.iov_len = sizeof(Arm32Regs); break;
        case Arch::kX86:    io.iov_len = sizeof(X86Regs);   break;
        default:            io.iov_len = 0;                 break;
    }
    return ptrace(PTRACE_GETREGSET, pid, reinterpret_cast<void*>(NT_PRSTATUS), &io) == 0;
}
bool setRegs(pid_t pid, Regs* r) {
    struct iovec io;
    io.iov_base = &r->u;
    switch (r->arch) {
        case Arch::kArm64:  io.iov_len = sizeof(Arm64Regs); break;
        case Arch::kX86_64: io.iov_len = sizeof(X64Regs);   break;
        case Arch::kArm32:  io.iov_len = sizeof(Arm32Regs); break;
        case Arch::kX86:    io.iov_len = sizeof(X86Regs);   break;
        default:            io.iov_len = 0;                 break;
    }
    return ptrace(PTRACE_SETREGSET, pid, reinterpret_cast<void*>(NT_PRSTATUS), &io) == 0;
}

uintptr_t getPc(const Regs* r) {
    switch (r->arch) {
        case Arch::kArm64: return r->u.a64.pc;
        case Arch::kX86_64: return r->u.x64.rip;
        case Arch::kArm32: return r->u.a32.uregs[15];
        case Arch::kX86: return r->u.x86.eip;
        default: return 0;
    }
}
void setPc(Regs* r, uintptr_t v) {
    switch (r->arch) {
        case Arch::kArm64: r->u.a64.pc = v; break;
        case Arch::kX86_64: r->u.x64.rip = v; break;
        case Arch::kArm32: r->u.a32.uregs[15] = static_cast<uint32_t>(v); break;
        case Arch::kX86: r->u.x86.eip = static_cast<uint32_t>(v); break;
        default: break;
    }
}
uintptr_t getSp(const Regs* r) {
    switch (r->arch) {
        case Arch::kArm64: return r->u.a64.sp;
        case Arch::kX86_64: return r->u.x64.rsp;
        case Arch::kArm32: return r->u.a32.uregs[13];
        case Arch::kX86: return r->u.x86.esp;
        default: return 0;
    }
}
void setSp(Regs* r, uintptr_t v) {
    switch (r->arch) {
        case Arch::kArm64: r->u.a64.sp = v; break;
        case Arch::kX86_64: r->u.x64.rsp = v; break;
        case Arch::kArm32: r->u.a32.uregs[13] = static_cast<uint32_t>(v); break;
        case Arch::kX86: r->u.x86.esp = static_cast<uint32_t>(v); break;
        default: break;
    }
}
uintptr_t getRetVal(const Regs* r) {
    switch (r->arch) {
        case Arch::kArm64: return r->u.a64.regs[0];
        case Arch::kX86_64: return r->u.x64.rax;
        case Arch::kArm32: return r->u.a32.uregs[0];
        case Arch::kX86: return r->u.x86.eax;
        default: return 0;
    }
}

// Cross-bitness ELF helpers (parse the target's ELF header / dynsym)

static uint16_t rd16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
static uint64_t rd64(const uint8_t* p) { return static_cast<uint64_t>(rd32(p)) | (static_cast<uint64_t>(rd32(p + 4)) << 32); }

struct ElfHdrInfo {
    bool is64 = false;
    uint16_t type = 0;
    uint16_t machine = 0;
    uint64_t entry = 0;
    uint64_t shoff = 0;
    uint16_t shentsize = 0;
    uint16_t shnum = 0;
    uint16_t shstrndx = 0;
};

bool readElfHdr(const uint8_t* p, size_t size, ElfHdrInfo* out) {
    if (size < 52 || memcmp(p, ELFMAG, SELFMAG) != 0) return false;
    out->is64 = (p[EI_CLASS] == ELFCLASS64);
    out->type = rd16(p + 16);
    out->machine = rd16(p + 18);
    if (out->is64) {
        if (size < 64) return false;
        out->entry = rd64(p + 24);
        out->shoff = rd64(p + 40);
        out->shentsize = rd16(p + 58);
        out->shnum = rd16(p + 60);
        out->shstrndx = rd16(p + 62);
    } else {
        out->entry = rd32(p + 24);
        out->shoff = rd32(p + 32);
        out->shentsize = rd16(p + 46);
        out->shnum = rd16(p + 48);
        out->shstrndx = rd16(p + 50);
    }
    return true;
}

Arch archFromElf(const ElfHdrInfo& h) {
    if (h.is64) {
        if (h.machine == EM_AARCH64) return Arch::kArm64;
        if (h.machine == EM_X86_64) return Arch::kX86_64;
    } else {
        if (h.machine == EM_ARM) return Arch::kArm32;
        if (h.machine == EM_386) return Arch::kX86;
    }
    return Arch::kUnknown;
}

// Resolve a dynamic symbol inside an ELF file (32- or 64-bit) mapped at `base`.
uintptr_t elfDlsym(const std::string& path, uintptr_t base, const char* symbol) {
    int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 0;
    struct stat st {};
    if (fstat(fd, &st) != 0 || st.st_size < 52) {
        close(fd);
        return 0;
    }
    size_t size = static_cast<size_t>(st.st_size);
    uint8_t* p = static_cast<uint8_t*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
    close(fd);
    if (p == MAP_FAILED) return 0;

    uintptr_t result = 0;
    ElfHdrInfo h;
    if (readElfHdr(p, size, &h) && h.shnum != 0 && h.shoff + static_cast<uint64_t>(h.shnum) * h.shentsize <= size) {
        auto shdr = [&](uint16_t i) -> const uint8_t* {
            return p + h.shoff + static_cast<uint64_t>(i) * h.shentsize;
        };
        auto sh_off = [&](const uint8_t* s) { return h.is64 ? rd64(s + 24) : rd32(s + 16); };
        auto sh_size = [&](const uint8_t* s) { return h.is64 ? rd64(s + 32) : rd32(s + 20); };
        auto sh_link = [&](const uint8_t* s) { return rd32(s + (h.is64 ? 40 : 24)); };
        auto sh_entsize = [&](const uint8_t* s) { return h.is64 ? rd64(s + 56) : rd32(s + 36); };

        for (uint16_t i = 0; i < h.shnum; ++i) {
            const uint8_t* s = shdr(i);
            uint32_t sh_type = rd32(s + 4);
            if (sh_type != SHT_DYNSYM && sh_type != SHT_SYMTAB) continue;

            uint64_t off = sh_off(s);
            uint64_t ent = sh_entsize(s);
            uint32_t link = sh_link(s);
            if (link >= h.shnum || ent == 0) continue;

            uint64_t stroff = sh_off(shdr(link));
            if (stroff >= size) continue;
            const char* strtab = reinterpret_cast<const char*>(p + stroff);

            uint64_t cnt = sh_size(s) / ent;
            for (uint64_t j = 0; j < cnt; ++j) {
                const uint8_t* sym = p + off + j * ent;
                uint32_t st_name = rd32(sym);
                uint16_t st_shndx = rd16(sym + (h.is64 ? 6 : 14));
                if (st_shndx == SHN_UNDEF) continue;
                if (st_name >= sh_size(shdr(link))) continue;
                if (strcmp(strtab + st_name, symbol) == 0) {
                    uint64_t st_value = h.is64 ? rd64(sym + 8) : rd32(sym + 4);
                    result = (h.type == ET_DYN) ? base + st_value : st_value;
                    goto done;
                }
            }
        }
    }
done:
    munmap(p, size);
    return result;
}

// Tracee state

enum State {
    STATE_TRACED = 0,
    STATE_ENTRY,    // breakpoint set at entry, waiting for linker->entry handoff
    STATE_MEMFD,    // memfd_create invoked, waiting for the fd
    STATE_DLOPEN,   // android_dlopen_ext invoked, waiting for its return
    STATE_DLSYM,    // dlsym(handle, "znn_loader_init") invoked
    STATE_INIT,     // znn_loader_init invoked
    STATE_DLERROR,  // dlerror invoked, waiting for its return through the breakpoint
};

struct Tracee {
    pid_t pid = -1;
    State state = STATE_TRACED;
    Arch arch = Arch::kUnknown;

    uintptr_t entry = 0;         // runtime entry point (LSB cleared for thumb)
    uint8_t orig_instr[8] = {0}; // instruction(s) overwritten by the breakpoint
    size_t bp_size = 0;          // breakpoint instruction size
    Regs saved;                  // registers captured when the entry was reached
    std::string loader;          // loader path (in the injector's filesystem)
    std::vector<uint8_t> content;  // loader bytes to be written into the memfd
    std::string exe;             // target executable path (for the WebUI state)
    uint64_t deadline_ms = 0;    // STATE_ENTRY timeout (poll-seized tracees)
};

std::map<pid_t, Tracee> g_tracees;

std::string readExePath(pid_t pid) {
    char link[64];
    snprintf(link, sizeof(link), "/proc/%d/exe", pid);
    char buf[PATH_MAX];
    ssize_t n = readlink(link, buf, sizeof(buf) - 1);
    if (n <= 0) return {};
    buf[n] = '\0';
    return buf;
}

bool findLibrary(pid_t pid, const char* basename64, const char* basename32, bool is64,
                 uintptr_t* base, std::string* path) {
    char p[64];
    snprintf(p, sizeof(p), "%d", pid);
    const char* want = is64 ? basename64 : basename32;
    for (const auto& m : parseMaps(p)) {
        if (m.offset != 0) continue;
        const char* bn = strrchr(m.path.c_str(), '/');
        bn = bn ? bn + 1 : m.path.c_str();
        if (strcmp(bn, want) == 0) {
            *base = m.start;
            *path = m.path;
            return true;
        }
    }
    return false;
}

// Resolve an exported symbol from a library named `basename` in the target.
uintptr_t resolveSymbol(pid_t pid, bool is64, const char* basename, const char* symbol) {
    uintptr_t base = 0;
    std::string path;
    if (findLibrary(pid, basename, basename, is64, &base, &path)) {
        return elfDlsym(path, base, symbol);
    }
    return 0;
}

uintptr_t resolveDlopenExt(pid_t pid, bool is64) {
    // The linker's __loader_android_dlopen_ext loads a library from a file
    // descriptor (ANDROID_DLEXT_USE_LIBRARY_FD), bypassing the namespace
    // "permitted path" check. The fd must live on tmpfs (a memfd): bionic
    // still re-checks namespace accessibility for fds on real filesystems,
    // which rejects anything under /data/adb.
    if (uintptr_t a = resolveSymbol(pid, is64, "linker64", "__loader_android_dlopen_ext")) return a;
    if (uintptr_t a = resolveSymbol(pid, is64, "linker", "__loader_android_dlopen_ext")) return a;
    if (uintptr_t a = resolveSymbol(pid, is64, "libdl.so", "android_dlopen_ext")) return a;
    return 0;
}

uintptr_t resolveDlerror(pid_t pid, bool is64) {
    return resolveSymbol(pid, is64, "libdl.so", "dlerror");
}

uintptr_t resolveSyscall(pid_t pid, bool is64) {
    return resolveSymbol(pid, is64, "libc.so", "syscall");
}

// __NR_memfd_create per architecture.
int sysMemfdCreate(Arch arch) {
    switch (arch) {
        case Arch::kArm64: return 279;
        case Arch::kX86_64: return 319;
        case Arch::kArm32: return 385;
        case Arch::kX86: return 356;
        default: return -1;
    }
}

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    struct stat st {};
    if (fstat(fd, &st) != 0 || st.st_size <= 0) {
        close(fd);
        return false;
    }
    out.resize(static_cast<size_t>(st.st_size));
    size_t off = 0;
    while (off < out.size()) {
        ssize_t n = read(fd, out.data() + off, out.size() - off);
        if (n <= 0) break;
        off += static_cast<size_t>(n);
    }
    close(fd);
    return off == out.size();
}

// Write bytes into a file descriptor owned by the tracee, via /proc/<pid>/fd/<fd>.
bool writeToTargetFd(pid_t pid, int fd, const void* data, size_t len) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/fd/%d", pid, fd);
    int wfd = open(path, O_WRONLY | O_CLOEXEC);
    if (wfd < 0) return false;
    const auto* p = static_cast<const uint8_t*>(data);
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(wfd, p + off, len - off);
        if (n <= 0) break;
        off += static_cast<size_t>(n);
    }
    close(wfd);
    return off == len;
}

std::string readCString(pid_t pid, uintptr_t addr) {
    std::string out;
    out.reserve(64);
    char c;
    while (out.size() < 512) {
        if (!readMem(pid, addr + out.size(), &c, 1)) break;
        if (c == '\0') break;
        out.push_back(c);
    }
    return out;
}

// Prepare a remote call to `func` with up to four register arguments, arranging
// for it to return to `ret` (which must still hold a breakpoint instruction).
bool setupCall(pid_t pid, Regs* r, uintptr_t func, uintptr_t ret, const uintptr_t* args, int nargs) {
    switch (r->arch) {
        case Arch::kArm64:
            for (int i = 0; i < nargs && i < 4; ++i) r->u.a64.regs[i] = args[i];
            r->u.a64.regs[30] = ret;  // lr
            setPc(r, func);
            return true;
        case Arch::kArm32:
            for (int i = 0; i < nargs && i < 4; ++i) r->u.a32.uregs[i] = static_cast<uint32_t>(args[i]);
            r->u.a32.uregs[14] = static_cast<uint32_t>(ret);  // lr
            setPc(r, func);
            return true;
        case Arch::kX86_64: {
            uintptr_t argv[4] = {0, 0, 0, 0};
            for (int i = 0; i < nargs && i < 4; ++i) argv[i] = args[i];
            r->u.x64.rdi = argv[0];
            r->u.x64.rsi = argv[1];
            r->u.x64.rdx = argv[2];
            r->u.x64.rcx = argv[3];
            uintptr_t sp = getSp(r) - 8;
            if (!writeMem(pid, sp, &ret, sizeof(ret))) return false;
            setSp(r, sp);
            setPc(r, func);
            return true;
        }
        case Arch::kX86: {
            // cdecl: push args right-to-left, then the return address.
            uintptr_t sp = getSp(r);
            uint32_t r32 = static_cast<uint32_t>(ret);
            sp -= 4;
            if (!writeMem(pid, sp, &r32, sizeof(r32))) return false;
            for (int i = nargs - 1; i >= 0; --i) {
                uint32_t a = static_cast<uint32_t>(args[i]);
                sp -= 4;
                if (!writeMem(pid, sp, &a, sizeof(a))) return false;
            }
            setSp(r, sp);
            setPc(r, func);
            return true;
        }
        default:
            return false;
    }
}

size_t breakpointBytes(Arch arch, bool thumb, uint8_t out[4]) {
    memset(out, 0, 4);
    switch (arch) {
        case Arch::kArm64:
            out[0] = 0x00; out[1] = 0x00; out[2] = 0x20; out[3] = 0xD4;  // brk #0
            return 4;
        case Arch::kX86_64:
        case Arch::kX86:
            out[0] = 0xCC;  // int3
            return 1;
        case Arch::kArm32:
            if (thumb) {
                out[0] = 0x00; out[1] = 0xBE;  // bkpt #0 (Thumb)
                return 2;
            }
            out[0] = 0x70; out[1] = 0x00; out[2] = 0x20; out[3] = 0xE1;  // bkpt #0 (ARM)
            return 4;
        default:
            return 0;
    }
}

bool setEntryBreakpoint(pid_t pid, Tracee* t, bool thumb) {
    uint8_t bp[4];
    size_t sz = breakpointBytes(t->arch, thumb, bp);
    if (sz == 0) return false;
    if (!readMem(pid, t->entry, t->orig_instr, sz)) return false;
    if (!writeMem(pid, t->entry, bp, sz)) return false;
    t->bp_size = sz;
    return true;
}

void restoreEntry(pid_t pid, const Tracee& t) {
    if (t.bp_size) writeMem(pid, t.entry, t.orig_instr, t.bp_size);
}

// First injection step: start a remote memfd_create("loader") call in the
// tracee. The rest of the flow (writing the bytes and calling
// android_dlopen_ext) continues in handleTrap.
bool injectAtEntry(pid_t pid, Tracee* t) {
    bool is64 = (t->arch == Arch::kArm64 || t->arch == Arch::kX86_64);
    uintptr_t syscall_addr = resolveSyscall(pid, is64);
    if (!syscall_addr) {
        LOGE("failed to resolve syscall for pid %d", pid);
        return false;
    }

    Regs regs = t->saved;

    const char name[] = "loader";
    uintptr_t sp = getSp(&regs);
    sp = (sp - sizeof(name) - 0x10) & ~0xFULL;
    if (!writeMem(pid, sp, name, sizeof(name))) return false;
    setSp(&regs, sp);  // keep the name above the callee's stack frame

    uintptr_t args[4] = {
        static_cast<uintptr_t>(sysMemfdCreate(t->arch)),
        sp,
        0,
        0,
    };
    if (!setupCall(pid, &regs, syscall_addr, t->entry, args, 3)) return false;
    if (!setRegs(pid, &regs)) return false;

    t->state = STATE_MEMFD;  // next: read the returned fd
    return true;
}

void handleExec(pid_t pid, Tracee& t) {
    std::string exe = readExePath(pid);
    if (exe.empty()) {
        ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
        g_tracees.erase(pid);
        return;
    }
    t.exe = exe;

    bool match = false;
    std::string name;
    {
        auto pos = exe.rfind('/');
        name = pos == std::string::npos ? exe : exe.substr(pos + 1);
    }
    for (const auto& target : g_targets) {
        if (target.is_name ? (name == target.value) : (exe == target.value)) {
            match = true;
            break;
        }
    }
    if (!match) {
        // Not a ZNN target: stop tracing this process (and thus its children).
        ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
        g_tracees.erase(pid);
        return;
    }

    // Diagnostic: the target's SELinux domain decides which of the module's
    // sepolicy rules are needed (memfd "unlabeled" reads, /data/adb traversal).
    {
        char attr[64];
        snprintf(attr, sizeof(attr), "/proc/%d/attr/current", pid);
        int afd = open(attr, O_RDONLY | O_CLOEXEC);
        if (afd >= 0) {
            char ctx[256];
            ssize_t n = read(afd, ctx, sizeof(ctx) - 1);
            close(afd);
            if (n > 0) {
                ctx[n] = '\0';
                LOGI("SELinux context of %s (pid %d): %s", exe.c_str(), pid, ctx);
            }
        }
    }

    int fd = open(("/proc/" + std::to_string(pid) + "/exe").c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        recordFailure(pid, exe, "cannot open /proc/pid/exe");
        ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
        g_tracees.erase(pid);
        return;
    }
    uint8_t hdr[64] = {0};
    ssize_t r = pread(fd, hdr, sizeof(hdr), 0);
    close(fd);
    ElfHdrInfo eh;
    if (r < 52 || !readElfHdr(hdr, static_cast<size_t>(r), &eh)) {
        recordFailure(pid, exe, "cannot read ELF header");
        ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
        g_tracees.erase(pid);
        return;
    }

    t.arch = archFromElf(eh);
    if (t.arch == Arch::kUnknown) {
        LOGW("skipping %s (pid %d): unsupported machine %u", exe.c_str(), pid, eh.machine);
        recordFailure(pid, exe, "unsupported ELF machine");
        ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
        g_tracees.erase(pid);
        return;
    }

    char p[64];
    snprintf(p, sizeof(p), "%d", pid);
    uintptr_t base = 0;
    for (const auto& m : parseMaps(p)) {
        if (m.offset == 0 && m.path == exe) {
            base = m.start;
            break;
        }
    }

    bool thumb = false;
    uintptr_t entry = eh.entry;
    if (t.arch == Arch::kArm32 && (entry & 1)) {
        thumb = true;
        entry &= ~1ULL;
    }
    entry = (eh.type == ET_DYN) ? base + entry : entry;
    if (!entry) {
        recordFailure(pid, exe, "no entry point");
        ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
        g_tracees.erase(pid);
        return;
    }

    t.entry = entry;
    t.loader = eh.is64 ? g_loader64 : g_loader32;
    t.state = STATE_ENTRY;
    if (!setEntryBreakpoint(pid, &t, thumb)) {
        recordFailure(pid, exe, "cannot set entry breakpoint");
        ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
        g_tracees.erase(pid);
        return;
    }

    LOGI("injecting %s into %s (pid %d) at entry %p (%s)", t.loader.c_str(), exe.c_str(), pid,
         reinterpret_cast<void*>(entry), eh.is64 ? "64-bit" : "32-bit");
    ptrace(PTRACE_CONT, pid, nullptr, nullptr);
}

void handleTrap(pid_t pid, Tracee& t) {
    if (t.state == STATE_ENTRY) {
        Regs regs;
        regs.arch = t.arch;
        if (!getRegs(pid, &regs)) {
            LOGE("failed to get regs at entry for pid %d: %s", pid, strerror(errno));
            restoreEntry(pid, t);
            ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
            g_tracees.erase(pid);
            return;
        }
        uintptr_t pc = getPc(&regs);
        uintptr_t expected_pc = t.entry;
        if (t.arch == Arch::kX86 || t.arch == Arch::kX86_64) {
            expected_pc += 1;
        } else if (t.arch == Arch::kArm32) {
            pc &= ~1ULL;
        }
        if (pc != expected_pc) {
            LOGE("unexpected entry state for pid %d (pc=%p, expected=%p)",
                 pid, reinterpret_cast<void*>(pc), reinterpret_cast<void*>(expected_pc));
            restoreEntry(pid, t);
            ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
            g_tracees.erase(pid);
            return;
        }
        t.saved = regs;
        if (!injectAtEntry(pid, &t)) {
            restoreEntry(pid, t);
            ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
            g_tracees.erase(pid);
            return;
        }
        ptrace(PTRACE_CONT, pid, nullptr, nullptr);
        return;
    }

    if (t.state == STATE_MEMFD) {
        // Unwind the injection: restore the entry instruction and registers, then
        // detach. A created memfd is deliberately left open (see loader.cpp).
        auto bail = [&](const char* reason) {
            LOGE("%s (pid %d)", reason, pid);
            recordFailure(pid, t.exe, reason);
            restoreEntry(pid, t);
            setRegs(pid, &t.saved);
            ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
            g_tracees.erase(pid);
        };

        Regs regs;
        regs.arch = t.arch;
        if (!getRegs(pid, &regs)) {
            bail("failed to get regs after memfd_create");
            return;
        }
        int memfd = static_cast<int>(getRetVal(&regs));
        if (memfd < 0) {
            LOGE("memfd_create failed for pid %d (%d)", pid, memfd);
            recordFailure(pid, t.exe, "memfd_create failed");
            restoreEntry(pid, t);
            setRegs(pid, &t.saved);
            ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
            g_tracees.erase(pid);
            return;
        }

        if (t.content.empty() && !readFile(t.loader, t.content)) {
            bail("cannot read the loader (injector side)");
            return;
        }
        if (!writeToTargetFd(pid, memfd, t.content.data(), t.content.size())) {
            LOGE("failed to write to memfd %d of pid %d", memfd, pid);
            recordFailure(pid, t.exe, "cannot write loader to memfd");
            restoreEntry(pid, t);
            setRegs(pid, &t.saved);
            ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
            g_tracees.erase(pid);
            return;
        }

        bool is64 = (t.arch == Arch::kArm64 || t.arch == Arch::kX86_64);
        uintptr_t dlopen_addr = resolveDlopenExt(pid, is64);
        if (!dlopen_addr) {
            bail("cannot resolve android_dlopen_ext");
            return;
        }

        // Build android_dlextinfo (USE_LIBRARY_FD + the memfd) and the name.
        // extinfo must be 8-byte aligned (uint64_t flags), so pad the name.
        uintptr_t sp = getSp(&regs);
        uint8_t extinfo[48] = {0};
        *reinterpret_cast<uint64_t*>(&extinfo[0]) = 0x10;  // ANDROID_DLEXT_USE_LIBRARY_FD
        // library_fd offset differs between the 32-bit (20) and 64-bit (28) ABIs.
        *reinterpret_cast<int32_t*>(&extinfo[is64 ? 28 : 20]) = memfd;
        const char name[] = "libloader.so";
        const size_t name_pad = (sizeof(name) + 7) & ~7ULL;  // round up to 8
        sp = (sp - name_pad - sizeof(extinfo) - 0x10) & ~0xFULL;
        uintptr_t name_addr = sp;
        uintptr_t extinfo_addr = sp + name_pad;
        if (!writeMem(pid, name_addr, name, sizeof(name)) ||
            !writeMem(pid, extinfo_addr, extinfo, sizeof(extinfo))) {
            bail("cannot write dlopen args");
            return;
        }
        setSp(&regs, sp);  // keep the args above the callee's stack frame

        uintptr_t args[4] = {name_addr, 2 /*RTLD_NOW*/, extinfo_addr, t.entry /*caller*/};
        if (!setupCall(pid, &regs, dlopen_addr, t.entry, args, 4) || !setRegs(pid, &regs)) {
            bail("cannot setup android_dlopen_ext");
            return;
        }
        t.state = STATE_DLOPEN;
        ptrace(PTRACE_CONT, pid, nullptr, nullptr);
        return;
    }

    if (t.state == STATE_DLOPEN) {
        Regs regs;
        regs.arch = t.arch;
        if (!getRegs(pid, &regs)) {
            restoreEntry(pid, t);
            ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
            g_tracees.erase(pid);
            return;
        }
        uintptr_t handle = getRetVal(&regs);
        if (handle) {
            LOGI("loader injected into pid %d (handle %p)", pid, reinterpret_cast<void*>(handle));

            // Resolve and call znn_loader_init() explicitly instead of from a
            // constructor: its statics may not be constructed yet in .init_array
            // order under LTO. Calling after dlopen returns guarantees they are.
            bool is64 = (t.arch == Arch::kArm64 || t.arch == Arch::kX86_64);
            uintptr_t dlsym_addr = resolveSymbol(pid, is64, "libdl.so", "dlsym");
            if (!dlsym_addr) {
                LOGE("failed to resolve dlsym for pid %d", pid);
                recordFailure(pid, t.exe, "cannot resolve dlsym");
                restoreEntry(pid, t);
                setRegs(pid, &t.saved);
                ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
                g_tracees.erase(pid);
                return;
            }

            const char name[] = "znn_loader_init";
            uintptr_t sp = getSp(&regs);
            sp = (sp - sizeof(name) - 0x10) & ~0xFULL;
            if (!writeMem(pid, sp, name, sizeof(name))) {
                LOGE("failed to write init symbol name for pid %d", pid);
                restoreEntry(pid, t);
                setRegs(pid, &t.saved);
                ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
                g_tracees.erase(pid);
                return;
            }
            setSp(&regs, sp);

            uintptr_t args[4] = {handle, sp, 0, 0};
            if (!setupCall(pid, &regs, dlsym_addr, t.entry, args, 2) || !setRegs(pid, &regs)) {
                LOGE("failed to setup dlsym for pid %d", pid);
                restoreEntry(pid, t);
                setRegs(pid, &t.saved);
                ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
                g_tracees.erase(pid);
                return;
            }
            t.state = STATE_DLSYM;
            ptrace(PTRACE_CONT, pid, nullptr, nullptr);
            return;
        }

        // dlopen failed: call dlerror() to learn the reason.
        bool is64 = (t.arch == Arch::kArm64 || t.arch == Arch::kX86_64);
        uintptr_t dlerror_addr = resolveDlerror(pid, is64);
        if (!dlerror_addr || !setupCall(pid, &regs, dlerror_addr, t.entry, nullptr, 0)) {
            LOGE("dlopen(%s) failed for pid %d (cannot resolve/call dlerror)",
                 t.loader.c_str(), pid);
            recordFailure(pid, t.exe, "dlopen failed (cannot resolve dlerror)");
            restoreEntry(pid, t);
            setRegs(pid, &t.saved);
            ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
            g_tracees.erase(pid);
            return;
        }
        setRegs(pid, &regs);
        t.state = STATE_DLERROR;
        ptrace(PTRACE_CONT, pid, nullptr, nullptr);
        return;
    }

    if (t.state == STATE_DLSYM) {
        Regs regs;
        regs.arch = t.arch;
        if (!getRegs(pid, &regs)) {
            LOGE("failed to get regs after dlsym for pid %d", pid);
            restoreEntry(pid, t);
            ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
            g_tracees.erase(pid);
            return;
        }
        uintptr_t init_fn = getRetVal(&regs);
        if (!init_fn) {
            LOGE("dlsym(znn_loader_init) failed for pid %d", pid);
            recordFailure(pid, t.exe, "dlsym znn_loader_init failed");
            restoreEntry(pid, t);
            setRegs(pid, &t.saved);
            ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
            g_tracees.erase(pid);
            return;
        }
        if (!setupCall(pid, &regs, init_fn, t.entry, nullptr, 0) || !setRegs(pid, &regs)) {
            LOGE("failed to setup znn_loader_init for pid %d", pid);
            restoreEntry(pid, t);
            setRegs(pid, &t.saved);
            ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
            g_tracees.erase(pid);
            return;
        }
        t.state = STATE_INIT;
        ptrace(PTRACE_CONT, pid, nullptr, nullptr);
        return;
    }

    if (t.state == STATE_INIT) {
        LOGI("loader initialized in pid %d", pid);
        recordSuccess(pid, t.exe);
        restoreEntry(pid, t);
        setRegs(pid, &t.saved);
        ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
        g_tracees.erase(pid);
        return;
    }

    if (t.state == STATE_DLERROR) {
        Regs regs;
        regs.arch = t.arch;
        if (!getRegs(pid, &regs)) {
            restoreEntry(pid, t);
            ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
            g_tracees.erase(pid);
            return;
        }
        uintptr_t err_ptr = getRetVal(&regs);
        std::string err = err_ptr ? readCString(pid, err_ptr) : "(null)";
        LOGE("dlopen(%s) failed for pid %d: %s", t.loader.c_str(), pid, err.c_str());
        recordFailure(pid, t.exe, "dlopen failed: " + err);
        if (err.find("Permission denied") != std::string::npos) {
            LOGE("hint for pid %d: the memfd is labeled \"tmpfs\" (GKI) or \"unlabeled\" "
                 "(other kernels) and the target domain cannot access it - make sure "
                 "sepolicy.rule is applied (allow * tmpfs/unlabeled file open read "
                 "getattr map execute)", pid);
        } else if (err.find("not accessible") != std::string::npos) {
            LOGE("hint for pid %d: the linker rejected the library path for its "
                 "namespace (loading must go through a tmpfs memfd)", pid);
        }

        restoreEntry(pid, t);
        setRegs(pid, &t.saved);
        ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
        g_tracees.erase(pid);
        return;
    }

    ptrace(PTRACE_CONT, pid, nullptr, nullptr);
}

// Classic mode (automatic when no Zygisk implementation is running) seizes
// init; see main(). PTRACE_O_EXITKILL is deliberately NOT set: if the
// injector dies we want init and its services to keep running (the kernel
// releases the tracees).
constexpr long kPtraceOpts = PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK | PTRACE_O_TRACECLONE |
                             PTRACE_O_TRACEEXEC | PTRACE_O_TRACEEXIT;

void attachChild(pid_t child) {
    if (g_tracees.count(child)) {
        return;
    }
    g_tracees[child] = Tracee{child, STATE_TRACED, Arch::kUnknown, 0, {0}, 0, {}, {}, {}, {}};
}

// Target-spawn discovery for compat mode (no ptrace on init)

// Targets that were already injected (or missed and given up on) for this pid.
std::set<pid_t> g_done;

// Pids evaluated and found irrelevant (never a target, not a pre-exec fork);
// skipped in later scans so their exe is not read-linked every poll.
std::set<pid_t> g_ignored;
uint32_t g_ignored_rescans = 0;

// Fresh forks of init being watched. `exe` is the last exe we saw; a fresh
// fork still shows init's own exe until it execs, so it is re-checked every
// poll until it either becomes a target (seize) or an irrelevant program.
struct Candidate {
    std::string exe;
    uint64_t target_since_ms = 0;  // when exe first matched a target (0 = never)
};
std::map<pid_t, Candidate> g_candidates;

static uint64_t nowMs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000 + static_cast<uint64_t>(ts.tv_nsec) / 1000000;
}

bool isTargetExe(const std::string& exe) {
    if (exe.empty()) return false;
    const auto pos = exe.rfind('/');
    const std::string name = pos == std::string::npos ? exe : exe.substr(pos + 1);
    for (const auto& t : g_targets) {
        if (t.is_name ? (name == t.value) : (exe == t.value)) return true;
    }
    return false;
}

// A freshly forked child of init still shows init's own exe until it execs.
bool isPreExecFork(const std::string& exe) {
    const auto pos = exe.rfind('/');
    const std::string name = pos == std::string::npos ? exe : exe.substr(pos + 1);
    return name == "init";
}

// Is `pc` inside the executable's own executable mappings? The executable's
// code only starts running at its entry point, so a PC inside its text means
// the process already passed entry and injection would be too late.
bool pcInExeText(pid_t pid, const std::string& exe, uintptr_t pc) {
    for (const auto& m : parseMaps(std::to_string(pid))) {
        if (m.path != exe) continue;
        if (!(m.perms & PROT_EXEC)) continue;
        if (pc >= m.start && pc < m.end) return true;
    }
    return false;
}

// Seize a target process that has just exec'd (still inside the dynamic
// linker) and plant the entry breakpoint. Returns:
//   kSeized  — breakpoint planted, injection will proceed at the entry trap
//   kRetry   — another tracer (e.g. a Zygisk monitor) still holds it; try
//              again on the next poll
//   kGiveUp  — permanent failure or already past entry
enum class SeizeResult { kSeized, kRetry, kGiveUp };

SeizeResult trySeizeTarget(pid_t pid, const std::string& exe) {
    int fd = open(("/proc/" + std::to_string(pid) + "/exe").c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        recordFailure(pid, exe, "cannot open /proc/pid/exe");
        return SeizeResult::kGiveUp;
    }
    uint8_t hdr[64] = {0};
    ssize_t r = pread(fd, hdr, sizeof(hdr), 0);
    close(fd);
    ElfHdrInfo eh;
    if (r < 52 || !readElfHdr(hdr, static_cast<size_t>(r), &eh)) {
        recordFailure(pid, exe, "cannot read ELF header");
        return SeizeResult::kGiveUp;
    }

    Arch arch = archFromElf(eh);
    if (arch == Arch::kUnknown) {
        LOGW("skipping %s (pid %d): unsupported machine %u", exe.c_str(), pid, eh.machine);
        recordFailure(pid, exe, "unsupported ELF machine");
        return SeizeResult::kGiveUp;
    }

    uintptr_t base = 0;
    for (const auto& m : parseMaps(std::to_string(pid))) {
        if (m.offset == 0 && m.path == exe) {
            base = m.start;
            break;
        }
    }
    if (!base) {
        recordFailure(pid, exe, "executable not mapped");
        return SeizeResult::kGiveUp;
    }
    bool thumb = false;
    uintptr_t entry = eh.entry;
    if (arch == Arch::kArm32 && (entry & 1)) {
        thumb = true;
        entry &= ~1ULL;
    }
    entry = (eh.type == ET_DYN) ? base + entry : entry;
    if (!entry) {
        recordFailure(pid, exe, "no entry point");
        return SeizeResult::kGiveUp;
    }

    if (ptrace(PTRACE_SEIZE, pid, nullptr, nullptr) != 0) {
        if (errno == EPERM || errno == EACCES) return SeizeResult::kRetry;
        recordFailure(pid, exe, "cannot seize");
        return SeizeResult::kGiveUp;
    }

    if (ptrace(PTRACE_INTERRUPT, pid, nullptr, nullptr) != 0) {
        recordFailure(pid, exe, "interrupt failed");
        ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
        return SeizeResult::kGiveUp;
    }
    // Wait for the interrupt stop, but bound the wait so a pathological state
    // (e.g. the process already group-stopped before we seized it) cannot hang
    // the polling loop.
    int status = 0;
    {
        const uint64_t stop_deadline = nowMs() + 200;
        for (;;) {
            pid_t w = waitpid(pid, &status, __WALL | WNOHANG);
            if (w == pid) break;
            if (w < 0 || nowMs() >= stop_deadline) {
                recordFailure(pid, exe, "interrupt wait timed out");
                ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
                return SeizeResult::kGiveUp;
            }
            usleep(500);
        }
    }
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        recordFailure(pid, exe, "process exited during seize");
        ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
        return SeizeResult::kGiveUp;
    }
    int cont_sig = 0;
    if (WIFSTOPPED(status)) {
        const int sig = WSTOPSIG(status);
        if (sig != SIGTRAP) cont_sig = sig;  // re-deliver a real pending signal
    }

    Regs regs;
    regs.arch = arch;
    if (!getRegs(pid, &regs)) {
        recordFailure(pid, exe, "cannot read registers");
        ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
        return SeizeResult::kGiveUp;
    }

    // Already past entry (running the executable's own code)? Too late.
    const uintptr_t pc = getPc(&regs);
    if (pcInExeText(pid, exe, pc)) {
        LOGW("%s (pid %d) already past entry (pc %p); instance missed", exe.c_str(), pid,
             reinterpret_cast<void*>(pc));
        recordFailure(pid, exe, "missed (already past entry)");
        ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
        return SeizeResult::kGiveUp;
    }

    Tracee t;
    t.pid = pid;
    t.state = STATE_ENTRY;
    t.arch = arch;
    t.entry = entry;
    t.loader = eh.is64 ? g_loader64 : g_loader32;
    t.exe = exe;
    t.deadline_ms = nowMs() + 3000;  // the linker must reach the entry soon
    if (!setEntryBreakpoint(pid, &t, thumb)) {
        recordFailure(pid, exe, "cannot set entry breakpoint");
        ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
        return SeizeResult::kGiveUp;
    }
    g_tracees[pid] = std::move(t);

    LOGI("seized %s (pid %d) mid-linker, entry breakpoint at %p (%s)", exe.c_str(), pid,
         reinterpret_cast<void*>(entry), eh.is64 ? "64-bit" : "32-bit");
    ptrace(PTRACE_CONT, pid, nullptr, reinterpret_cast<void*>(static_cast<uintptr_t>(cont_sig)));
    return SeizeResult::kSeized;
}

// Handle one newly observed (or re-checked) target pid.
void observeTarget(pid_t pid, const std::string& exe) {
    SeizeResult r = trySeizeTarget(pid, exe);
    if (r == SeizeResult::kSeized) {
        g_done.insert(pid);
        g_candidates.erase(pid);
    } else if (r == SeizeResult::kGiveUp) {
        g_done.insert(pid);
        g_candidates.erase(pid);
    }
    // kRetry: keep the candidate; trySeizeTarget is cheap, retry next poll.
}

// Fast path (compat mode): full /proc scan, parent-agnostic. Runs every poll
// (~2 ms). New pids are evaluated once; tracked pre-exec forks and targets
// held by another tracer are re-checked every scan.
void pollProcesses() {
    std::set<pid_t> seen;
    DIR* d = opendir("/proc");
    if (!d) return;
    struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] < '0' || de->d_name[0] > '9') continue;
        const pid_t pid = static_cast<pid_t>(strtol(de->d_name, nullptr, 10));
        if (pid <= 1) continue;
        seen.insert(pid);

        if (g_tracees.count(pid) || g_done.count(pid)) continue;
        if (g_ignored.count(pid)) continue;  // already judged irrelevant

        std::string exe = readExePath(pid);
        if (exe.empty()) continue;  // zombie / gone — pruned below
        const bool target = isTargetExe(exe);

        auto it = g_candidates.find(pid);
        if (it == g_candidates.end()) {
            // First sighting. Remember it only when it is worth re-checking:
            // a pre-exec fork (still shows its parent's exe, will exec later)
            // or an already-exec'd target (seize now). Everything else is
            // ignored so it is not read-linked again on every scan.
            if (target) {
                Candidate c;
                c.exe = exe;
                c.target_since_ms = nowMs();
                g_candidates[pid] = std::move(c);
                observeTarget(pid, exe);
            } else if (isPreExecFork(exe)) {
                Candidate c;
                c.exe = exe;
                g_candidates[pid] = std::move(c);
            } else {
                g_ignored.insert(pid);
            }
            continue;
        }

        // Tracked candidate: re-check its exe every scan.
        if (exe == it->second.exe) {
            if (target) {
                if (nowMs() - it->second.target_since_ms > 5000) {
                    // Held by another tracer for too long — the process must be
                    // running its main() by now; stop retrying.
                    recordFailure(pid, exe, "held by another tracer for too long");
                    g_done.insert(pid);
                    g_candidates.erase(it);
                } else {
                    observeTarget(pid, exe);
                }
            }
            continue;
        }

        // exe changed: the fork exec'd. Evaluate the new program.
        it->second.exe = exe;
        it->second.target_since_ms = target ? nowMs() : 0;
        if (target) {
            observeTarget(pid, exe);
        } else {
            g_candidates.erase(it);  // exec'd to an irrelevant program
            g_ignored.insert(pid);
        }
    }
    closedir(d);

    for (auto it = g_candidates.begin(); it != g_candidates.end();) {
        if (!seen.count(it->first)) it = g_candidates.erase(it);
        else ++it;
    }
    for (auto it = g_ignored.begin(); it != g_ignored.end();) {
        if (!seen.count(*it)) it = g_ignored.erase(it);
        else ++it;
    }
    for (auto it = g_done.begin(); it != g_done.end();) {
        if (!seen.count(*it)) it = g_done.erase(it);
        else ++it;
    }

    // Re-evaluate previously ignored pids every ~30 s: a pid that ran an
    // irrelevant program could later execve() a target (rare but possible).
    if (++g_ignored_rescans >= 15000) {  // 15000 * 2 ms ≈ 30 s
        g_ignored_rescans = 0;
        g_ignored.clear();
    }
}

// Give up on poll-seized tracees whose entry breakpoint never fired.
void expirePending() {
    const uint64_t now = nowMs();
    for (auto it = g_tracees.begin(); it != g_tracees.end();) {
        Tracee& t = it->second;
        if (t.state == STATE_ENTRY && t.deadline_ms != 0 && now >= t.deadline_ms) {
            const pid_t pid = it->first;
            LOGW("entry breakpoint for pid %d never hit (already past entry?), detaching", pid);
            recordFailure(pid, t.exe, "entry breakpoint never hit");
            restoreEntry(pid, t);
            ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
            g_done.insert(pid);
            it = g_tracees.erase(it);
        } else {
            ++it;
        }
    }
}

// Is a Zygisk-family daemon running? (Magisk Zygisk's zygiskd, NeoZygisk's
// zygisk-core64 / zygiskd64, ReZygisk, ...). Such frameworks hold init with
// PTRACE_O_TRACEFORK, so we must not compete for pid 1.
bool zygiskPresent() {
    DIR* d = opendir("/proc");
    if (!d) return false;
    bool found = false;
    struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] < '0' || de->d_name[0] > '9') continue;
        char path[64];
        snprintf(path, sizeof(path), "/proc/%s/comm", de->d_name);
        int fd = open(path, O_RDONLY | O_CLOEXEC);
        if (fd < 0) continue;
        char comm[64] = {0};
        const ssize_t n = read(fd, comm, sizeof(comm) - 1);
        close(fd);
        if (n > 0 && strstr(comm, "zygisk")) {
            found = true;
            break;
        }
    }
    closedir(d);
    return found;
}

void handleEvent(pid_t pid, int status) {
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        g_tracees.erase(pid);
        return;
    }
    if (!WIFSTOPPED(status)) return;

    int sig = WSTOPSIG(status);

    if (sig == SIGTRAP) {
        int event = (status >> 16) & 0xffff;
        if (event == PTRACE_EVENT_STOP) {
            ptrace(PTRACE_CONT, pid, nullptr, nullptr);
            return;
        }
        switch (event) {
            case PTRACE_EVENT_FORK:
            case PTRACE_EVENT_VFORK:
            case PTRACE_EVENT_CLONE: {
                unsigned long child = 0;
                if (ptrace(PTRACE_GETEVENTMSG, pid, nullptr, &child) == 0 && child) {
                    attachChild(static_cast<pid_t>(child));
                }
                ptrace(PTRACE_CONT, pid, nullptr, nullptr);
                break;
            }
            case PTRACE_EVENT_EXEC: {
                auto it = g_tracees.find(pid);
                if (it != g_tracees.end()) handleExec(pid, it->second);
                else ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
                break;
            }
            case PTRACE_EVENT_EXIT: {
                g_tracees.erase(pid);
                ptrace(PTRACE_CONT, pid, nullptr, nullptr);
                break;
            }
            default: {
                auto it = g_tracees.find(pid);
                if (it != g_tracees.end()) handleTrap(pid, it->second);
                else ptrace(PTRACE_CONT, pid, nullptr, nullptr);
                break;
            }
        }
        return;
    }

    if (sig == SIGSTOP || sig == SIGCHLD) {
        ptrace(PTRACE_CONT, pid, nullptr, nullptr);
        return;
    }

    // The tracee crashed. crash_dump cannot attach to it (we are already
    // attached), so no tombstone is produced — report the fault location
    // ourselves before resuming with the signal.
    if (sig == SIGSEGV || sig == SIGBUS || sig == SIGABRT || sig == SIGILL || sig == SIGFPE ||
        sig == SIGSYS) {
        auto it = g_tracees.find(pid);
        if (it != g_tracees.end()) {
            Tracee& t = it->second;
            Regs regs;
            regs.arch = t.arch;
            if (getRegs(pid, &regs)) {
                uintptr_t pc = getPc(&regs);
                uintptr_t sp = getSp(&regs);
                uintptr_t lr = 0;
                if (t.arch == Arch::kArm64) lr = regs.u.a64.regs[30];
                else if (t.arch == Arch::kArm32) lr = regs.u.a32.uregs[14];

                std::string loc = "(unmapped)";
                for (const auto& m : parseMaps(std::to_string(pid))) {
                    if (pc >= m.start && pc < m.end) {
                        char off[32];
                        snprintf(off, sizeof(off), "+0x%lx", static_cast<unsigned long>(pc - m.start));
                        loc = m.path + off;
                        break;
                    }
                }
                LOGE("fatal signal %d in pid %d at pc=%p lr=%p sp=%p in %s", sig, pid,
                     reinterpret_cast<void*>(pc), reinterpret_cast<void*>(lr),
                     reinterpret_cast<void*>(sp), loc.c_str());
            }
        }
    }

    ptrace(PTRACE_CONT, pid, nullptr, reinterpret_cast<void*>(sig));
}

}  // namespace

int main(int argc, char** argv) {
    // WebUI control mode: `injector --ctl <status|system|modules|rescan>`.
    // Runs as a short-lived client that echoes the daemon's state snapshot; it
    // never scans /proc on demand and needs no module-dir argument.
    if (argc > 2 && strcmp(argv[1], "--ctl") == 0) {
        return ctlMain(argv[2]);
    }

    if (argc > 1) {
        g_module_dir = argv[1];
    } else {
        char self[PATH_MAX];
        ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
        if (n > 0) {
            self[n] = '\0';
            std::string s = self;
            auto pos = s.rfind('/');
            if (pos != std::string::npos) {
                s = s.substr(0, pos);
                pos = s.rfind('/');
                if (pos != std::string::npos) g_module_dir = s.substr(0, pos);
            }
        }
    }

    // The loader content is read by the injector (running as root) and injected
    // into targets via memfd, so the path only matters for the injector itself.
    g_loader64 = g_module_dir + "/lib64/libloader.so";
    g_loader32 = g_module_dir + "/lib/libloader.so";

    LOGI("Zygisk Next Next %s starting", ZNN_VERSION);

    // Diagnostics: are the loaders present and what labels do they carry?
    // (The label decides which SELinux rules the target needs for its own
    // module reads; the memfd injection itself only needs the "unlabeled" rule.)
    for (const char* p : {g_loader64.c_str(), g_loader32.c_str()}) {
        char ctx[256];
        ssize_t n = lgetxattr(p, "security.selinux", ctx, sizeof(ctx) - 1);
        if (access(p, R_OK) != 0) {
            LOGE("loader %s not readable: %s", p, strerror(errno));
        } else if (n < 0) {
            LOGE("loader %s: cannot read selinux label: %s", p, strerror(errno));
        } else {
            ctx[n] = '\0';
            LOGI("loader %s label %s", p, ctx);
        }
    }

    signal(SIGHUP, on_sighup);
    collectSystemInfo();
    collectTargets();
    LOGI("collected %zu zn modules", g_targets.size());

    // Choose the mode automatically. Classic (trace init) is deterministic but
    // incompatible with Zygisk implementations, which hold pid 1 for their
    // whole session (Linux permits only one tracer per process). If a Zygisk
    // daemon is already running, or init is already held, use compat mode
    // (poll /proc, never touch init).
    if (zygiskPresent()) {
        LOGI("will enable zygisk compat mode (poll /proc)");
        g_mode = 1;
    } else {
        LOGI("tracing init (pid 1)");
        if (ptrace(PTRACE_SEIZE, 1, nullptr, reinterpret_cast<void*>(kPtraceOpts)) != 0) {
            LOGW("cannot seize init: %s — another tracer holds pid 1; "
                 "using compat mode (poll /proc) instead", strerror(errno));
            g_mode = 1;
        } else {
            g_tracees[1] = Tracee{1, STATE_TRACED, Arch::kUnknown, 0, {0}, 0, {}, {}, {}, {}};
            LOGI("successfully seized init");
        }
    }

    time_t last_rescan = 0;
    uint64_t last_zygisk_check = 0;
    uint64_t last_state_write_ms = 0;

    // Publish the initial snapshot right away (pid/mode/system/modules), so the
    // WebUI has data even before the first periodic rescan.
    writeStateSnapshot();
    last_state_write_ms = nowMs();

    for (;;) {
        if (g_rescan) {
            g_rescan = 0;
            collectTargets();
            last_rescan = time(nullptr);
            g_state_dirty = true;
            LOGI("rescanned znn targets (%zu)", g_targets.size());
        }

        if (g_mode == 1) {
            // Compat mode: poll every ~2 ms for newly spawned targets.
            pollProcesses();
        } else if (nowMs() - last_zygisk_check >= 5000) {
            // Classic mode: a Zygisk implementation may have started after us
            // and is now failing to seize init. Yield init to it and switch
            // to compat mode so both frameworks keep working.
            last_zygisk_check = nowMs();
            if (zygiskPresent()) {
                LOGW("Zygisk implementation started while tracing init; yielding init and "
                     "switching to compat mode");
                ptrace(PTRACE_DETACH, 1, nullptr, nullptr);
                g_tracees.erase(1);
                g_mode = 1;
            }
        }

        // Reap ptrace events (entry traps, memfd/dlopen/init callbacks, ...).
        // WNOHANG keeps the polling loop responsive; ECHILD (no tracees right
        // now) is normal and must NOT terminate the daemon.
        for (;;) {
            int status;
            pid_t pid = waitpid(-1, &status, __WALL | WNOHANG);
            if (pid == 0) break;
            if (pid < 0) {
                if (errno == EINTR) continue;
                if (errno == ECHILD) break;
                LOGE("waitpid failed: %s", strerror(errno));
                break;
            }
            handleEvent(pid, status);
        }

        // Give up on tracees whose entry breakpoint never fired.
        expirePending();

        time_t tnow = time(nullptr);
        if (tnow - last_rescan >= 5) {
            collectTargets();
            last_rescan = tnow;
            g_state_dirty = true;
        }

        // Persist the WebUI snapshot: immediately after a rescan or when events
        // (injection success/failure) dirtied it, throttled to ~1/s so a burst
        // of events does not spam the disk. The 5s rescan above also forces a
        // write even without events (mode/pid changes, process deaths).
        const uint64_t now = nowMs();
        if (g_state_dirty && (now - last_state_write_ms >= 1000 || last_state_write_ms == 0)) {
            writeStateSnapshot();
            g_state_dirty = false;
            last_state_write_ms = now;
        }

        usleep(g_mode == 1 ? 2000 : 1000);
    }
    return 0;
}
