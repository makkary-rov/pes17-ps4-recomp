<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# R0 allowlist

R0 is the publish rule that keeps title-specific names and terms out of the repository.
`scripts/audit_publish.py` applies it to tracked and untracked files and to commit messages.

## Rules
1. The R0 pattern is the one in `scripts/audit_publish.py`. It is never weakened.
2. Only QA adds an entry to the block below. The implementor and architect do not.
3. An entry is an exact match: the file path, a TAB, and the exact line text (CR and LF stripped).
   A matching line is exempt; every other line in that file is still checked.
4. An entry needs a reason in the commit or QA report. An empty block is the normal state.

## Allowlist

The block below is read by the audit. It is empty.

```allow
```

## Known false positives of wider searches (not R0 hits)

These lines match broader searches used during M1d review. The R0 pattern does not match them,
so they need no allowlist entry.

- `src/import_names.inc:349`: an encoded import identifier that contains a two-letter upper-case run; not an R0 term.
- `gpu/shadps4/shader_recompiler/frontend/format.cpp:20`, `:601`, `:1441` and `gpu/shadps4/shader_recompiler/frontend/decode.cpp:105`: instruction mnemonics (`SUBB_U32`, `UBB_U32`, `V_SUBB_U32`); the R0 pattern requires a word boundary before the run, which these do not have.
- `gpu/VENDOR.txt:8`: an upstream commit note that names an instruction mnemonic; not an R0 term.
- `build.sh:9`: a Nix store path in a compiler search loop (`/nix/store/...`); a path, not a title term.
