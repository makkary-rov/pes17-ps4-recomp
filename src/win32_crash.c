// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "crash_win.h"

static const void *guest_base;
static size_t guest_size;
static volatile LONG counter;

void crash_win_set_guest(const void *base, size_t size) {
    guest_base = base;
    guest_size = size;
}

void crash_win_init(void) {}

unsigned crash_win_protect_secret(const void *p, size_t n) {
    (void)p;
    (void)n;
    return (unsigned)InterlockedIncrement(&counter);
}

void crash_win_unprotect_secret(unsigned id) {
    (void)id;
}

/* Minidump of the first crash of the process, kept for later review: every thread's stack and
 * registers plus the memory they point at (a corrupted heap block next to a bad pointer), not
 * the whole guest memory. dbghelp is loaded only here. PES_NO_MINIDUMP=1 turns it off. */
typedef BOOL(WINAPI *MiniDumpWriteDumpFn)(HANDLE, DWORD, HANDLE, int, void *, void *, void *);
typedef struct {
    DWORD ThreadId;
    EXCEPTION_POINTERS *ExceptionPointers;
    BOOL ClientPointers;
} PesMinidumpExceptionInfo;
enum {
    PES_MINIDUMP_WITH_DATA_SEGS = 0x1,
    PES_MINIDUMP_WITH_INDIRECTLY_REFERENCED_MEMORY = 0x40,
    PES_MINIDUMP_WITH_PROCESS_THREAD_DATA = 0x100,
    PES_MINIDUMP_WITH_THREAD_INFO = 0x1000,
};
static volatile LONG dumped;

int crash_win_write_minidump(const char *path, EXCEPTION_POINTERS *exception) {
    HMODULE dbghelp = LoadLibraryA("dbghelp.dll");
    MiniDumpWriteDumpFn write =
        dbghelp ? (MiniDumpWriteDumpFn)(void *)GetProcAddress(dbghelp, "MiniDumpWriteDump") : NULL;
    if (!write) return 0;
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    PesMinidumpExceptionInfo info = {GetCurrentThreadId(), exception, FALSE};
    const int type = PES_MINIDUMP_WITH_DATA_SEGS | PES_MINIDUMP_WITH_INDIRECTLY_REFERENCED_MEMORY |
                     PES_MINIDUMP_WITH_PROCESS_THREAD_DATA | PES_MINIDUMP_WITH_THREAD_INFO;
    const BOOL ok = write(GetCurrentProcess(), GetCurrentProcessId(), file, type,
                          exception ? &info : NULL, NULL, NULL);
    CloseHandle(file);
    if (!ok) DeleteFileA(path);
    return ok ? 1 : 0;
}

static void keep_minidump(EXCEPTION_POINTERS *exception) {
    const char *off = getenv("PES_NO_MINIDUMP");
    if ((off && off[0] == '1') || InterlockedCompareExchange(&dumped, 1, 0) != 0) return;
    CreateDirectoryA("user", NULL);
    CreateDirectoryA("user\\crashes", NULL);
    SYSTEMTIME t;
    GetLocalTime(&t);
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "user\\crashes\\crash-%04u%02u%02u-%02u%02u%02u-%lu.dmp", t.wYear,
             t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, (unsigned long)GetCurrentProcessId());
    if (crash_win_write_minidump(path, exception)) {
        fprintf(stderr, "Crash: minidump kept as %s\n", path);
    } else {
        fprintf(stderr, "Crash: minidump not written (error %lu)\n", (unsigned long)GetLastError());
    }
    fflush(stderr);
}

static void report(DWORD pid, DWORD tid, EXCEPTION_POINTERS *e) {
    if (!e || !e->ExceptionRecord) {
        fprintf(stderr, "Crash: no exception record\n");
        fflush(stderr);
        return;
    }
    const EXCEPTION_RECORD *r = e->ExceptionRecord;
    const unsigned long long rip = e->ContextRecord ? (unsigned long long)e->ContextRecord->Rip : 0;
    fprintf(stderr, "Crash: pid %lu tid %lu code 0x%08lx address %p rip 0x%llx",
            (unsigned long)pid, (unsigned long)tid, (unsigned long)r->ExceptionCode,
            r->ExceptionAddress, rip);
    const uintptr_t base = (uintptr_t)guest_base;
    if (guest_base && rip >= base && rip - base < guest_size) {
        fprintf(stderr, " guest offset 0x%llx", (unsigned long long)(rip - base));
    }
    fprintf(stderr, "\n");
    fflush(stderr);
}

void crash_win_dump(EXCEPTION_POINTERS *exception) {
    report(GetCurrentProcessId(), GetCurrentThreadId(), exception);
    keep_minidump(exception);
}

void crash_win_dump_external(HANDLE process, DWORD pid, DWORD tid, EXCEPTION_POINTERS *exception) {
    (void)process;
    (void)exception;
    fprintf(stderr, "Crash: external process\n");
    fprintf(stderr, "Crash: pid %lu tid %lu\n", (unsigned long)pid, (unsigned long)tid);
    fflush(stderr);
}

void crash_win_dump_fatal(const char *reason) {
    fprintf(stderr, "Fatal: %s (pid %lu tid %lu)\n", reason ? reason : "(null)",
            (unsigned long)GetCurrentProcessId(), (unsigned long)GetCurrentThreadId());
    fflush(stderr);
    keep_minidump(NULL);
}
#endif
