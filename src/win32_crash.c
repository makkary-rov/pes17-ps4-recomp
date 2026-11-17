// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#include <stdint.h>
#include <stdio.h>
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
}
#endif
