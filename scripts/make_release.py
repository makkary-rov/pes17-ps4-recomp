# SPDX-License-Identifier: GPL-2.0-or-later
"""Builds the Windows release zip: the three programs, the DLLs they need, the preparation scripts, a
bundled Python (so nobody has to install Python or MSYS2), the licence texts and a notice.

Run in the MSYS2 CLANG64 shell after ./build.sh, with the embeddable Python package from python.org
(python-3.x.y-embed-amd64.zip, downloaded by the person who makes the release):

    python scripts/make_release.py --python-embed <python-embed-amd64.zip> [--version 0.1.0] [--out dist]

The zip holds no game files. Players still need their own copy of the game. The bundled DLLs come
from the MSYS2 packages the programs were built with; their licences are listed in NOTICE-RELEASE.txt.
"""
import argparse
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROGRAMS = ("pes17launcher.exe", "pes17settings.exe", "pes17recomp.exe")
SCRIPTS = ("prepare.py", "content_profile.py", "link_modules.py", "link_libc.py", "title_config.py")
DOCS = ("LICENSE", "NOTICE", "README.md", "LEGAL.md", "CREDITS.md", "pes17recomp.ini.example")
DLL_RE = re.compile(r"DLL Name:\s*(\S+)", re.I)

NOTICE = """2017RECOMP, Windows release
===========================

What this is
  Programs that run the PS4 build of a 2017 football game on Windows. The zip contains no game files:
  you need your own copy of the PS4 game (the folder with eboot.bin). Start pes17launcher.exe, press
  Install, choose that folder, and the launcher prepares it once with the bundled Python.

Source code
  The complete source of everything in this zip, except the third-party libraries and Python named
  below, is the repository this release was published from (see README.md). A copy of the source for
  the exact version is available from the same page.

Licences
  The project's own files: GPL-2.0-or-later (LICENSE, LICENSES\\).
  The DLLs next to the programs come from the MSYS2 CLANG64 packages: SDL3 (zlib), FFmpeg
  (avcodec, avformat, avutil, swresample, swscale: the package is built --enable-gpl --enable-version3,
  so this zip as a whole is distributed under GPL-3.0-or-later terms), Zydis (MIT), xxHash (BSD-2-Clause),
  libc++ and libunwind (Apache-2.0 with LLVM exceptions), and their own dependencies. Sources of those
  packages: https://packages.msys2.org/ (each package page links its source and licence).
  python\\ is the embeddable Python from python.org, under the Python Software Foundation licence
  (python\\LICENSE.txt). It is used only for the one-time preparation of the game.

Trademarks
  Pro Evolution Soccer, PES, PlayStation and related names, logos and marks belong to their owners
  and are named only to identify the game. This project is not affiliated with or endorsed by them.
"""


def dll_deps(exe_paths, search_dir, objdump):
    """DLLs (found in search_dir) that the programs need, directly or through each other."""
    found, queue = {}, list(exe_paths)
    while queue:
        item = queue.pop()
        out = subprocess.run([objdump, "-p", str(item)], capture_output=True, text=True, check=True).stdout
        for name in DLL_RE.findall(out):
            if name.lower() in found:
                continue
            candidate = search_dir / name
            if not candidate.is_file():
                for other in search_dir.glob("*.dll"):
                    if other.name.lower() == name.lower():
                        candidate = other
                        break
            if candidate.is_file():
                found[name.lower()] = candidate
                queue.append(candidate)
    return sorted(found.values(), key=lambda p: p.name.lower())


def add_python(embed_zip, dest):
    """Unpacks the embeddable Python into dest and lets it import the scripts next to it."""
    with zipfile.ZipFile(embed_zip) as z:
        z.extractall(dest)
    pth = list(dest.glob("python*._pth"))
    if len(pth) != 1:
        sys.exit("the Python package has no python*._pth file: use the embeddable zip from python.org")
    lines = pth[0].read_text(encoding="utf-8").splitlines()
    if "..\\scripts" not in lines:
        lines.insert(0, "..\\scripts")
    pth[0].write_text("\n".join(lines) + "\n", encoding="utf-8")
    if not (dest / "python.exe").is_file():
        sys.exit("python.exe is missing from the Python package")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--python-embed", required=True, type=Path, help="python-3.x.y-embed-amd64.zip from python.org")
    ap.add_argument("--version", default="0.1.0")
    ap.add_argument("--out", type=Path, default=ROOT / "dist")
    ap.add_argument("--dll-dir", type=Path, help="folder with the MSYS2 DLLs (default: the folder of objdump)")
    args = ap.parse_args()

    objdump = shutil.which("objdump")
    if not objdump:
        sys.exit("objdump not found: run this in the MSYS2 CLANG64 shell")
    dll_dir = args.dll_dir or Path(objdump).parent
    for name in PROGRAMS:
        if not (ROOT / "out" / name).is_file():
            sys.exit(f"out/{name} is missing: run ./build.sh first")
    if not args.python_embed.is_file():
        sys.exit(f"{args.python_embed} not found")

    name = f"2017RECOMP-{args.version}-windows"
    stage = args.out / name
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)
    for exe in PROGRAMS:
        shutil.copy2(ROOT / "out" / exe, stage / exe)
    dlls = dll_deps([ROOT / "out" / exe for exe in PROGRAMS], dll_dir, objdump)
    for dll in dlls:
        shutil.copy2(dll, stage / dll.name)
    (stage / "scripts").mkdir()
    for script in SCRIPTS:
        shutil.copy2(ROOT / "scripts" / script, stage / "scripts" / script)
    (stage / "src" / "title").mkdir(parents=True)
    shutil.copy2(ROOT / "src" / "title" / "CUSA04957.json", stage / "src" / "title" / "CUSA04957.json")
    for doc in DOCS:
        shutil.copy2(ROOT / doc, stage / doc)
    shutil.copytree(ROOT / "LICENSES", stage / "LICENSES")
    (stage / "NOTICE-RELEASE.txt").write_text(NOTICE, encoding="utf-8")
    add_python(args.python_embed, stage / "python")

    archive = args.out / (name + ".zip")
    if archive.exists():
        archive.unlink()
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for path in sorted(stage.rglob("*")):
            if path.is_file():
                z.write(path, Path(name) / path.relative_to(stage))
    print(f"PASS: {archive} ({archive.stat().st_size // 1024} KiB, {len(dlls)} DLLs)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
