# SPDX-License-Identifier: GPL-2.0-or-later
"""Publish guard over history (M7): every blob reachable from a branch, every commit message and every
author and committer. Run: python scripts/audit_history.py [ref]   (default: the current branch)

Applies the same rules as audit_publish.py to every blob that ever existed on the branch: game-derived
names, oversized binaries, R0 terms in text (with the same exemptions), personal e-mail addresses in our
own files; commit messages against both R0 patterns; authors and committers must be the project identity.
Exit 0 = clean.
"""
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import audit_publish as ap  # noqa: E402

ROOT = ap.ROOT


def git(*args, data=None):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, input=data, check=True).stdout


def main():
    ref = sys.argv[1] if len(sys.argv) > 1 else "HEAD"
    allow_path = ROOT / "docs" / "R0-ALLOWLIST.md"
    allow = ap.parse_allowlist(allow_path.read_text(encoding="utf-8")) if allow_path.exists() else set()
    paths = {}
    for line in git("rev-list", "--objects", ref).decode("utf-8", "replace").splitlines():
        sha, _, path = line.partition(" ")
        if path:
            paths.setdefault(sha, set()).add(path)
    info = git("cat-file", "--batch-check=%(objectname) %(objecttype) %(objectsize)",
               data=("\n".join(paths) + "\n").encode()).decode().splitlines()
    blobs = [line.split()[0] for line in info if " blob " in line]
    findings = set()
    for sha in blobs:
        data = git("cat-file", "blob", sha)
        for rel in paths[sha]:
            for reason in ap.check_name(rel) + ap.check_size(rel, data):
                findings.add(f"FAIL {reason} {rel}")
            if b"\0" not in data[:8192]:
                text = data.decode("utf-8", "replace")
                for lineno, _ in ap.r0_hits(rel, text, allow):
                    findings.add(f"FAIL r0 {rel}:{lineno}")
                for lineno, _ in ap.email_hits(rel, text):
                    findings.add(f"FAIL email {rel}:{lineno}")
    for record in git("log", ref, "--format=%H%x1f%ae%x1f%ce%x1f%B%x00").decode("utf-8", "replace").split("\0"):
        if not record.strip():
            continue
        sha, author, committer, message = record.strip("\n").split("\x1f", 3)
        if ap.message_bad(message):
            findings.add(f"FAIL commit-message {sha[:12]}")
        if author != ap.IDENTITY or committer != ap.IDENTITY:
            findings.add(f"FAIL identity {sha[:12]}")
    for line in sorted(findings):
        print(line)
    if findings:
        print(f"FAIL: {len(findings)} findings in {len(blobs)} blobs of {ref}")
        return 1
    print(f"PASS: audit_history clean ({len(blobs)} blobs of {ref})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
