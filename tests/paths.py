# SPDX-License-Identifier: GPL-2.0-or-later
"""Repository paths for the Python tests (run: python3 -m unittest discover -s tests)."""
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
if str(ROOT / 'scripts') not in sys.path:
    sys.path.insert(0, str(ROOT / 'scripts'))

WINDOWS = os.name == 'nt' or sys.platform in ('cygwin', 'msys')


def exe(name):
    """Test binary under out/: name.exe on Windows (MSYS2 CLANG64 writes .exe), else name."""
    return ROOT / 'out' / (name + '.exe' if WINDOWS else name)
