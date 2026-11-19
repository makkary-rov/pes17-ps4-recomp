/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Read-only state observer (ledger row G-44) for CUSA04957: where the match controller keeps its mode.
 * Evidence (existing Ghidra project, read-only; guest = Ghidra address - 0x1000000):
 *   guest 0x2c1bf0 (match-scene factory) stores the controller it creates (size 0x16d78, created by
 *   guest 0x239f840) in the global at guest 0x4996218.
 *   guest 0x23a3dd0 (SetMode) writes the mode byte at controller+0xb8 and clears the sub-state;
 *   guest 0x23a3dc0 writes the sub-state byte at controller+0xb9.
 * The observer only reads these bytes; it never writes guest memory or changes control flow. */
#ifndef PES_TITLE_CUSA04957_OBSERVER_H
#define PES_TITLE_CUSA04957_OBSERVER_H

#define PES_OBSERVER_CONTROLLER_GLOBAL 0x4996218u  /* image offset of the controller pointer */
#define PES_OBSERVER_MODE_OFFSET 0xb8u               /* controller byte: mode (15 = match, 16/18 = kickoff path) */
#define PES_OBSERVER_SUB_OFFSET 0xb9u                /* controller byte: sub-state */
/* The observer reads mode and sub-state in one 2-byte read: they must be adjacent. */
_Static_assert(PES_OBSERVER_SUB_OFFSET == PES_OBSERVER_MODE_OFFSET + 1, "observer reads mode and sub together");

#endif
