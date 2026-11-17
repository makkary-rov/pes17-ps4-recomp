<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Contributing

## Patch records

Every game-specific patch needs a patch record in `docs/patch-records.md` using the template there:
ID, guest address and range, function, original-bytes hash, change, evidence, root cause or workaround, test, status.
A patch is applied only if its original-bytes hash matches.

A fix needs a test that fails before the fix. No test that fails before the fix means no fix.

## Before every pull request

1. Run `python3 scripts/audit_publish.py`. It must print PASS (warnings are allowed where a record says so).
2. Run `python3 -m unittest discover -s tests` and the build tests in [BUILDING.md](BUILDING.md).
3. Run the py_compile gate: `git ls-files -z '*.py' | xargs -0 python3 -m py_compile`.

Do not add title-specific names or terms to new code, file names or documentation. Use "upstream title" instead.

## Trace rule

Diagnostics go behind `PES_*` environment variables and are off by default. Do not leave a diagnostic enabled in a default path.

## Licences

New files start with an SPDX-License-Identifier line. Vendored code keeps its licence notes in `LICENSES/`.
