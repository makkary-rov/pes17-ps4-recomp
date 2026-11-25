# SPDX-License-Identifier: GPL-2.0-or-later
"""Compares two guest image snapshots (PES_SNAP_DIR, see src/main.c) as 4-byte words.

    python scripts/snapdiff.py A.bin B.bin [A2.bin] [--limit N]

Prints the words that differ between A and B (offset = guest address, old -> new, as int and float).
If A2 (a second snapshot taken at the same setting as A) is given, words that also differ between A and
A2 are noise and are left out.
"""
import struct
import sys


def words(path):
    with open(path, "rb") as f:
        data = f.read()
    return data, len(data) // 4


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    limit = 200
    if "--limit" in sys.argv:
        limit = int(sys.argv[sys.argv.index("--limit") + 1])
        args = [a for a in args if a != str(limit)]
    a, n = words(args[0])
    b, nb = words(args[1])
    n = min(n, nb)
    noise = None
    if len(args) > 2:
        c, nc = words(args[2])
        n = min(n, nc)
        noise = c
    shown = 0
    changed = 0
    for i in range(n):
        o = i * 4
        if a[o:o + 4] == b[o:o + 4]:
            continue
        if noise is not None and a[o:o + 4] != noise[o:o + 4]:
            continue
        changed += 1
        if shown < limit:
            old, new = struct.unpack_from("<I", a, o)[0], struct.unpack_from("<I", b, o)[0]
            fo, fn = struct.unpack_from("<f", a, o)[0], struct.unpack_from("<f", b, o)[0]
            print(f"{o:08x}  {old:#010x} -> {new:#010x}   ({old} -> {new}; {fo:.6g} -> {fn:.6g})")
            shown += 1
    print(f"{changed} words differ (showing {shown})")


if __name__ == "__main__":
    main()
