/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Trace facility (plan section 8, ledger row T-1).
 *
 * PES_TRACE=loader,file,gpu,title,kernel,patch  enables subsystems (default: all off).
 * The mask is read once by pes_trace_init(); hot paths never call getenv. PES_TRACE_FILE names the
 * output file (default stderr). One structured line per event:
 *   t=<ms since init> sub=<name> ev=<event> <key=value ...>
 * Every subsystem has a line cap (PES_TRACE_CAP, default 20000). Probes never write guest memory and
 * never change control flow. Build with -DPES_NO_TRACE to compile the facility out. */
#ifndef PES_RUNTIME_TRACE_H
#define PES_RUNTIME_TRACE_H

#include <stdint.h>

enum {
    PES_TRACE_LOADER = 1u << 0,
    PES_TRACE_FILE   = 1u << 1,
    PES_TRACE_GPU    = 1u << 2,
    PES_TRACE_TITLE  = 1u << 3,
    PES_TRACE_KERNEL = 1u << 4,
    PES_TRACE_PATCH  = 1u << 5
};

#ifdef PES_NO_TRACE
#define pes_trace_init(spec) ((void)(spec))
#define pes_trace_enabled(sub) 0
#define pes_trace(sub, event, ...) ((void)0)
#define pes_trace_shutdown() ((void)0)
static inline uint32_t pes_trace_parse(const char *spec) { (void)spec; return 0; }
#else
/* Parse a comma separated subsystem list into a mask; unknown names are ignored; "all" = every subsystem. */
uint32_t pes_trace_parse(const char *spec);
/* spec: value of PES_TRACE (NULL = off). Reads PES_TRACE_FILE and PES_TRACE_CAP once. */
void pes_trace_init(const char *spec);
void pes_trace_shutdown(void);
extern uint32_t pes_trace_mask;
#define pes_trace_enabled(sub) ((pes_trace_mask & (sub)) != 0)
/* One line per event; the format arguments are key=value pairs. */
void pes_trace_emit(uint32_t sub, const char *event, const char *fmt, ...);
#define pes_trace(sub, event, ...) \
    do { if (pes_trace_enabled(sub)) pes_trace_emit((sub), (event), __VA_ARGS__); } while (0)
#endif

#endif
