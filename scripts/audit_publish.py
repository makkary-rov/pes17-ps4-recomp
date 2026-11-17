# SPDX-License-Identifier: GPL-2.0-or-later
"""Publish guard (R0 and file rules). Run: python3 scripts/audit_publish.py

Checks git-tracked and untracked (non-ignored) files for game-derived names, oversized
binaries and R0 terms, and checks commit messages for R0 terms. Exit 0 = clean.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PENDING = {"gpu/shim/pes_gnm_table.inc": "U-4, drop at M2"}
TEXT_SUFFIXES = {".c", ".h", ".cpp", ".hpp", ".inc", ".py", ".sh", ".md", ".txt", ".json", ".cmake",
                 ".glsl", ".comp", ".frag", ".vert", ".ini", ".example", ".yml", ".patch", ".bat", ".ps1"}
TEXT_NAMES = {"LICENSE", "NOTICE", "CMakeLists.txt"}
# The fragments are joined at run time so this file does not match its own patterns.
# The compiled patterns are the plan's R0 patterns, unchanged.
P = re.compile('|'.join(['blood' + 'borne', 'bb' + 'gpu', 'bb' + 'port', 'bb' + 'host', 'bb' + 'net',
                         'bb' + 'probe', 'bb-' + 'probe', 'bb-' + 'engine', 'bb-' + 'crash', r'\bbb' + '_',
                         'hun' + 'ter', 'yhar' + 'nam', 'cusa' + '03173', r'\.' + 'sl2' + r'\b']), re.I)
Q = re.compile('|'.join([r'\bBB' + '[A-Z_]', r'\bBb' + '[A-Z]']))


def is_text(rel, data):
    path = Path(rel)
    return (path.suffix.lower() in TEXT_SUFFIXES or path.name in TEXT_NAMES) and b"\0" not in data[:8192]


def check_name(rel):
    low = rel.lower()
    parts = low.split("/")
    base = parts[-1]
    reasons = []
    if re.search(r'^eboot\.', base):
        reasons.append("eboot")
    if re.search(r'^boot.*\.bin$', base):
        reasons.append("boot-binary")
    if re.search(r'\.dmp$', base):
        reasons.append("dump")
    if re.search(r'\.' + 'sl2$', base):
        reasons.append("save-file")
    if re.search(r'\.sav$', base):
        reasons.append("save-file")
    if re.search(r'^param\.sfo$', base):
        reasons.append("param-sfo")
    if any(part in {"savedata", "saves", "save"} for part in parts):
        reasons.append("save-directory")
    if low.startswith("src/title/generated/") or re.search(r'_gnm_table\.inc$', base):
        reasons.append("generated-table")
    if not low.startswith("licenses/") and (P.search(rel) or Q.search(rel)):
        reasons.append("r0-name")
    return reasons


def check_size(rel, data):
    if len(data) > 1 << 20 and not is_text(rel, data):
        return ["oversized-binary"]
    return []


def r0_hits(rel, text, allow):
    if rel in ("NOTICE", "CREDITS.md") or rel.lower().startswith("licenses/"):
        return []
    hits = []
    for lineno, line in enumerate(text.splitlines(), 1):
        clean = line.rstrip("\r\n")
        if (rel, clean) in allow:
            continue
        if P.search(line) or Q.search(line):
            hits.append((lineno, clean))
    return hits


def parse_allowlist(md):
    allow = set()
    inside = False
    for raw in md.splitlines():
        line = raw.rstrip("\r\n")
        if not inside:
            if line == "```allow":
                inside = True
            continue
        if line == "```":
            inside = False
            continue
        if "\t" in line:
            path, _, text = line.partition("\t")
            allow.add((path, text))
    return allow


def git_files():
    found = set()
    for args in (["git", "ls-files", "-z"], ["git", "ls-files", "-z", "--others", "--exclude-standard"]):
        out = subprocess.run(args, cwd=ROOT, capture_output=True, check=True).stdout
        found.update(p.decode("utf-8", "replace") for p in out.split(b"\0") if p)
    return sorted(found)


def commit_messages():
    out = subprocess.run(["git", "log", "--all", "--format=%H%x1f%B%x00"], cwd=ROOT,
                         capture_output=True, check=True).stdout
    commits = []
    for record in out.decode("utf-8", "replace").split("\0"):
        if not record.strip():
            continue
        sha, _, message = record.partition("\x1f")
        commits.append((sha.strip(), message))
    return commits


def main():
    allow_path = ROOT / "docs" / "R0-ALLOWLIST.md"
    allow = parse_allowlist(allow_path.read_text(encoding="utf-8")) if allow_path.exists() else set()
    lines = []
    fails = 0
    count = 0
    for rel in git_files():
        path = ROOT / rel
        if path.is_dir() or not path.exists():
            continue
        data = path.read_bytes()
        count += 1
        for rule, reasons in (("name", check_name(rel)), ("size", check_size(rel, data))):
            if not reasons:
                continue
            if rel in PENDING:
                lines.append(f"WARN {rule} {rel} ({PENDING[rel]})")
            else:
                lines.append(f"FAIL {rule} {rel}")
                fails += 1
        if b"\0" not in data[:8192]:
            text = data.decode("utf-8", "replace")
            for lineno, _ in r0_hits(rel, text, allow):
                lines.append(f"FAIL r0 {rel}:{lineno}")
                fails += 1
    for sha, message in commit_messages():
        if P.search(message):
            lines.append(f"FAIL commit-message {sha[:12]}")
            fails += 1
    for line in lines:
        print(line)
    if fails:
        print(f"FAIL: {fails} findings")
        return 1
    print(f"PASS: audit_publish clean ({count} files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
