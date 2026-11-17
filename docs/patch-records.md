<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Patch records

Each game-specific patch has one record. Records use new names only ("upstream title" for the game).
This file holds the template. No records are listed yet.

| Field | Content |
|---|---|
| ID | P-xxx, ledger row ID |
| Guest address / range | e.g. 0x22d8bb3, length |
| Function | FUN_xxxxxxxx and role as understood |
| Original-bytes hash | SHA-256 of the original bytes (patch applied only if it matches; otherwise refuse and log) |
| Change | new bytes / HLE behaviour |
| Why (evidence) | dump name (private) + our summary; log line; what the PS4 would have done |
| Root cause or workaround | if workaround: the root-cause task ID |
| Test | harness scenario and pass condition |
| Status | proposed / approved (QA) / removed |
