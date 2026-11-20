# SPDX-License-Identifier: GPL-2.0-or-later
"""scripts/make_release.py: the bundled Python is set up so it can import the preparation scripts."""
import tempfile
import unittest
import zipfile
from pathlib import Path

from paths import ROOT  # noqa: F401  (puts scripts/ on sys.path)
import make_release as mr


class AddPythonTests(unittest.TestCase):
    def make_zip(self, directory, members):
        path = Path(directory) / "embed.zip"
        with zipfile.ZipFile(path, "w") as z:
            for name, text in members.items():
                z.writestr(name, text)
        return path

    def test_scripts_folder_is_added_to_the_path_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            embed = self.make_zip(tmp, {"python.exe": "x", "python313._pth": "python313.zip\n.\n"})
            dest = Path(tmp) / "python"
            mr.add_python(embed, dest)
            lines = (dest / "python313._pth").read_text(encoding="utf-8").splitlines()
            self.assertEqual(lines[0], "..\scripts")
            self.assertIn(".", lines)
            mr.add_python(embed, dest)  # a second run does not add it twice
            again = (dest / "python313._pth").read_text(encoding="utf-8").splitlines()
            self.assertEqual(again.count("..\scripts"), 1)

    def test_a_package_without_a_path_file_is_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            embed = self.make_zip(tmp, {"python.exe": "x"})
            with self.assertRaises(SystemExit):
                mr.add_python(embed, Path(tmp) / "python")


if __name__ == "__main__":
    unittest.main()
