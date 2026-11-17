/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Patch engine (ledger row G-32) and read-only state observer (G-44).
 *
 * A game-specific patch needs a record in docs/patch-records.md and a test that fails without it.
 * An entry is applied only if the SHA-256 of the original bytes at its offset matches; otherwise it is
 * refused and logged (PES_TRACE=patch). The table is data: no address is hard-coded in this file. The
 * observer reads guest memory and never writes it. */
#ifndef PES_RUNTIME_PATCH_H
#define PES_RUNTIME_PATCH_H

#include <stddef.h>
#include <stdint.h>

#define PES_PATCH_MAX_LENGTH 4096

typedef struct {
    const char *id;               /* record id, e.g. "P-012" */
    uint64_t offset;              /* image offset (guest address) */
    uint32_t length;              /* 1..PES_PATCH_MAX_LENGTH */
    const char *original_sha256;  /* 64 hex digits: SHA-256 of the original bytes */
    const uint8_t *replacement;   /* length bytes */
} pes_patch;

typedef struct {
    unsigned applied;
    unsigned refused;
} pes_patch_report;

void pes_sha256(const uint8_t *data, size_t length, uint8_t digest[32]);
/* Returns the number of refused entries (0 = every entry applied). Refused entries change nothing. */
unsigned pes_patch_apply(uint8_t *image, size_t image_size, const pes_patch *table, size_t count,
                         pes_patch_report *report);

typedef struct {
    const char *name;
    uint64_t offset;
    uint8_t size;                 /* 1, 2, 4 or 8 bytes, little endian */
} pes_watch;

typedef struct {
    uint64_t last;
    uint8_t seen;
} pes_watch_state;

/* Logs every watch whose value changed since the previous sample (PES_TRACE=title). Returns the number of
 * changes. image is const: the observer cannot write. */
unsigned pes_observer_sample(const uint8_t *image, size_t image_size, const pes_watch *watches, size_t count,
                             pes_watch_state *state);

#endif
