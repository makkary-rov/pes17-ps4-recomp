# verify/

## verify_M2.ps1 (milestone M2 check)

Runs the M2 criteria C1-C9 (spec: `.agent-work/2026-11-17-m2-plan/spec.md`, section 1 and the
Amendments) against one game directory and writes `out\verify\M2-result.json`. Run it on the PC
that has the game:

```
powershell -ExecutionPolicy Bypass -File verify\verify_M2.ps1 -Game <game dir>
```

Prerequisites:

- MSYS2 at `C:\msys64` (or `MSYS2_ROOT`) with the CLANG64 toolchain used by `build.sh`.
- Python 3 on PATH (`python`, `py -3` or `python3`).
- GNU objdump for C7a. Install it once in an MSYS2 shell with `pacman -S binutils`; this puts
  `usr\bin\objdump.exe`, which the script looks at first. Without it C7a is SKIP and the overall
  result is INCOMPLETE. LLVM objdump (`clang64\bin`) is refused, because it has no `-b binary`.
- The game directory is only read. Do not run `pes17recomp.exe` while the script runs.

Options:

| Option | Meaning |
|---|---|
| `-Game <dir>` | Game directory (holds `eboot.elf` or `eboot.bin`, and `sce_sys\param.sfo`). Required. |
| `-SkipBuild` | Do not run `build.sh`; use the existing `out\pes17recomp.exe`. |
| `-TitleConfig <json>` | Title config. Default `src\title\CUSA04957.json`. |
| `-AllowHashMismatch` | Pass `--allow-hash-mismatch` to `prepare.py` (the mismatch is recorded). |
| `-Out <dir>` | Output root. Default `out`; artifacts go to `<Out>\verify`. |

What it runs, in order: `build.sh` in the MSYS2 CLANG64 shell (C:\msys64, or `MSYS2_ROOT`);
`prepare.py` twice (C1); `content_profile.py`; the loader with `--cpu-only --stop-before-entry
--import-aliases --import-report --dump-image --content-profile` (C2, C3, C5, C6); the relocation
and title checks (C4, C8); the GNM table generator (C7a, from the image dump and GNU objdump); and
the agent-side gate: `build.sh --test`, `py_compile` of the tracked `.py` files, `unittest discover`
and `audit_publish.py` (C9). Each step is logged to `out\verify\M2-script.log` and a failing step
does not stop the rest.

Outputs (all under `out\verify\`, git-ignored):

- `M2-result.json`: schema `pes-verify-m2-1`. Top-level `overall` (PASS, EXPLAIN, INCOMPLETE or
  FAIL; precedence below) and `summary` counts. Per criterion: `id`, `status` (PASS, FAIL, EXPLAIN,
  SKIP or NA), `detail`, `values`. Top-level `steps` lists each step; `arguments` steps report bad
  path arguments. Also the input hashes (MD5 and SHA-256 of the input file, SHA-256
  of `param.sfo`), the executable and `boot.bin` SHA-256, CPU, OS, Python and objdump versions, and
  start, end and elapsed time. It holds no game bytes.
- `M2-imports.tsv`: the import report (one row per import).
- `M2-loader.log`: the loader's stdout and stderr.
- `M2-*.log`: logs of the other steps. `run1\` and `run2\` are the two `prepare.py` outputs.
  `gnm\` holds the disassembly and generator intermediates of C7a.

The `overall` field of `M2-result.json` takes one of four values, with this precedence:
FAIL > INCOMPLETE > EXPLAIN > PASS.

- FAIL: at least one criterion is FAIL.
- INCOMPLETE: no FAIL, but at least one criterion is SKIP (not run, for example C7a without GNU
  objdump or C9 without MSYS2). NA does not count as SKIP.
- EXPLAIN: no FAIL and no SKIP, but at least one criterion is EXPLAIN (C4, C8). Each EXPLAIN must
  be read and explained before sign-off.
- PASS: every criterion is PASS or NA.

The exit code is 1 if overall is FAIL, otherwise 0. INCOMPLETE and EXPLAIN exit 0 but must be read
(see the criteria below).

If a path argument (`-Game`, `-TitleConfig` or `-Out`) has illegal path characters, the script
records an `arguments` FAIL step, uses the default `out` for the result, and still writes
`out\verify\M2-result.json`.

### Criteria and how to read them

- C1 PASS: two `prepare.py` runs give identical `boot.bin` and `analysis.json`.
- C2 PASS: `analysis.json` entry equals the title config (0x910), the loader prints the matching
  `Entering ... guest offset` line, and the loader exits 0.
- C3 PASS: SHA-256 of the image dump equals `analysis.json` `image_sha256`. The dump is deleted
  after use.
- C4 PASS if `relocations_written` equals the title config. Otherwise EXPLAIN: the count must be
  explained in `knowledge.md` before sign-off.
- C5 PASS: the `Imports:` summary line matches the TSV, every import is in the TSV, and
  `prefix_only` is 0. FAIL lists the first 20 offending names; this is expected on the first run
  before the suffix map is filled (task T7).
- C6 PASS: the loader exits 0 and does not report missing CPU features (`this CPU lacks ...`).
  FAIL when it reports missing features, and also when the loader exits non-zero without that
  message (it stopped before or during the CPU check; the exit code is recorded). The CPU name is
  recorded.
- C7a PASS: the GNM table is generated from the title's `gnm_region`, with rows. It needs GNU
  objdump (`-b binary`); see Prerequisites. LLVM objdump (the one in `C:\msys64\clang64\bin`) is
  refused, so C7a is SKIP with the reason unless a GNU objdump is on `MSYS2_ROOT\usr\bin` or on PATH.
  The C7a PASS branch has not been rehearsed (the synthetic ELF has no `.eh_frame`), so on run #1
  spot-check one region row of the generated `src\title\generated\pes_gnm_table.inc` against
  Ghidra: pick one row, find its guest address in the existing Ghidra project (Ghidra address minus
  0x1000000) and compare the instruction bytes and mnemonic. Record the result in the run notes.
- C7b NA: the Windows build has no GNM binding census (it moves to M4).
- C8: the title id is CUSA04957 and matches the content profile. The loader run passes `--app0` with
  the game directory, and the loader log then holds `User directory: <path>` (startup) and
  `Save root: <user>/savedata/1/CUSA04957`. C8 PASS when the save root is `<user>\savedata\1\CUSA04957`
  for the reported user directory, lies under the repo `user` directory and not under the game
  directory. EXPLAIN when the loader did not run, the log has no such lines, or the user directory is
  not the repo `user` directory (for example `PES_USER_DIR` or `--user` set). FAIL when the ids differ,
  `param.sfo` `INSTALL_DIR_SAVEDATA` names another directory, or the save root is inside the game
  directory.
- C9 PASS: `build.sh --test`, `py_compile`, `unittest` and `audit_publish` (0 WARN) all exit 0.
  SKIP when MSYS2 is not installed.

### Rehearsal without game files

A synthetic game (plain ELF made by `tests/test_prepare.py`) with a matching title config
exercises every step without any game bytes. It was run with `-SkipBuild`. The synthetic
title id is not CUSA04957, so C8 FAILs by design, and C7a is SKIP without GNU objdump.
