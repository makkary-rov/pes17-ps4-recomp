// SPDX-License-Identifier: GPL-2.0-or-later
// pes: sampling profiler, see pes_profiler.h.
#include "pes_profiler.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <timeapi.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace PesProfiler {
namespace {

constexpr int kDepth = 32;

struct Target {
    DWORD id;
    HANDLE handle;
    std::string name;
};

std::vector<std::string> Parts() {
    const char* env = std::getenv("PES_PROFILE");
    std::string spec = env && env[0] && std::strcmp(env, "1") != 0 ? env : "GpuCommandProcessor";
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= spec.size()) {
        const size_t comma = spec.find(',', start);
        const std::string part = spec.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        if (!part.empty()) parts.push_back(part);
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return parts;
}

std::string Narrow(const wchar_t* text) {
    std::string out;
    for (; text && *text; ++text) out.push_back(*text < 128 ? char(*text) : '?');
    return out;
}

// The matching threads of this process (re-scanned every few seconds: threads start late).
void Refresh(std::vector<Target>& targets, const std::vector<std::string>& parts) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    const DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid || te.th32ThreadID == self) continue;
        if (std::any_of(targets.begin(), targets.end(), [&](const Target& t) { return t.id == te.th32ThreadID; })) continue;
        HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_LIMITED_INFORMATION, FALSE,
                              te.th32ThreadID);
        if (!h) continue;
        PWSTR desc = nullptr;
        std::string name;
        if (SUCCEEDED(GetThreadDescription(h, &desc)) && desc) {
            name = Narrow(desc);
            LocalFree(desc);
        }
        const bool match = std::any_of(parts.begin(), parts.end(),
                                       [&](const std::string& p) { return name.find(p) != std::string::npos; });
        if (match) {
            targets.push_back({te.th32ThreadID, h, name});
            std::printf("Profiler: sampling thread %lu '%s'\n", te.th32ThreadID, name.c_str());
        } else {
            CloseHandle(h);
        }
    }
    CloseHandle(snap);
}

// One stack of a suspended thread, walked with the x64 unwind data. Returns the frame count.
// No allocation here: the thread may hold the heap lock.
int Walk(HANDLE thread, uint64_t* pcs) {
    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(thread, &ctx)) return 0;
    int n = 0;
    while (n < kDepth && ctx.Rip) {
        pcs[n++] = ctx.Rip;
        DWORD64 base = 0;
        PRUNTIME_FUNCTION fe = RtlLookupFunctionEntry(ctx.Rip, &base, nullptr);
        if (!fe) break; // a leaf or code without unwind data (guest code): stop here
        PVOID handler_data = nullptr;
        DWORD64 establisher = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, ctx.Rip, fe, &ctx, &handler_data, &establisher, nullptr);
    }
    return n;
}

void Dump(const char* path, const std::map<std::vector<uint64_t>, uint64_t>& stacks,
          const std::vector<Target>& targets, const std::map<DWORD, uint64_t>& per_thread, uint64_t total) {
    std::vector<std::pair<uint64_t, const std::vector<uint64_t>*>> order;
    order.reserve(stacks.size());
    for (const auto& [stack, count] : stacks) order.push_back({count, &stack});
    std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    const std::string tmp = std::string(path) + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "w");
    if (!f) return;
    std::fprintf(f, "# pes profile: %llu samples, 1 ms interval; module %p\n", (unsigned long long)total,
                 (void*)GetModuleHandleW(nullptr));
    for (const auto& t : targets) {
        const auto it = per_thread.find(t.id);
        std::fprintf(f, "# thread %lu '%s' %llu samples\n", t.id, t.name.c_str(),
                     (unsigned long long)(it == per_thread.end() ? 0 : it->second));
    }
    for (const auto& [count, stack] : order) {
        std::fprintf(f, "%llu t%llu", (unsigned long long)count, (unsigned long long)(*stack)[0]);
        for (size_t i = 1; i < stack->size(); ++i) std::fprintf(f, " %llx", (unsigned long long)(*stack)[i]);
        std::fputc('\n', f);
    }
    std::fclose(f);
    std::remove(path);
    std::rename(tmp.c_str(), path);
}

void Run() {
    const std::vector<std::string> parts = Parts();
    const char* out_env = std::getenv("PES_PROFILE_OUT");
    const std::string out = out_env && out_env[0] ? out_env : "pes17-profile.txt";
    std::printf("Profiler: on, threads matching PES_PROFILE, output %s\n", out.c_str());
    std::vector<Target> targets;
    std::map<std::vector<uint64_t>, uint64_t> stacks;
    std::map<DWORD, uint64_t> per_thread;
    uint64_t total = 0;
    timeBeginPeriod(1);
    DWORD last_scan = 0, last_dump = GetTickCount();
    uint64_t pcs[kDepth];
    for (;;) {
        const DWORD now = GetTickCount();
        if (now - last_scan > 2000 || last_scan == 0) {
            Refresh(targets, parts);
            last_scan = now;
        }
        for (const auto& t : targets) {
            if (SuspendThread(t.handle) == DWORD(-1)) continue;
            const int n = Walk(t.handle, pcs);
            ResumeThread(t.handle);
            if (n == 0) continue;
            std::vector<uint64_t> key(pcs, pcs + n);
            key.insert(key.begin(), uint64_t(t.id)); // first: the thread
            ++stacks[key];
            ++per_thread[t.id];
            ++total;
        }
        if (now - last_dump > 10000) {
            Dump(out.c_str(), stacks, targets, per_thread, total);
            last_dump = now;
        }
        Sleep(1);
    }
}

} // namespace

void Start() {
    if (!std::getenv("PES_PROFILE")) return;
    std::thread(Run).detach();
}

} // namespace PesProfiler

#else

namespace PesProfiler {
void Start() {}
}

#endif
