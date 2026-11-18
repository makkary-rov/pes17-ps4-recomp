// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef PES_CRASH_WIN_H
#define PES_CRASH_WIN_H
#ifdef _WIN32
#include <windows.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PES_CRASH_ASSERT_EXCEPTION ((DWORD)0xe0424201u)
void crash_win_init(void);
/* Redact known credential pages; tokens belong to their owner, not to a peer. */
unsigned crash_win_protect_secret(const void *, size_t);
void crash_win_unprotect_secret(unsigned);
void crash_win_set_guest(const void *base, size_t size);
void crash_win_dump(EXCEPTION_POINTERS *exception);
/* The debugger keeps the child stopped and its handle open until this returns. */
void crash_win_dump_external(HANDLE process, DWORD pid, DWORD tid, EXCEPTION_POINTERS *exception);
/* Capture a deliberate fatal stop without raising/reclassifying a CPU fault. */
void crash_win_dump_fatal(const char *reason);
/* Writes a minidump of this process (threads, registers, referenced memory) to path; 1 on success.
 * crash_win_dump and crash_win_dump_fatal keep one per process under user\crashes. */
int crash_win_write_minidump(const char *path, EXCEPTION_POINTERS *exception);
#ifdef __cplusplus
}
#endif
#endif
#endif
