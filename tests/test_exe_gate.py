# SPDX-License-Identifier: GPL-2.0-or-later
from paths import ROOT, exe
import unittest

TEST_BINARIES = ('content-test', 'runtime-test', 'sema-test')


class ExeGateTests(unittest.TestCase):
    def test_built_test_binaries_are_found_so_their_suites_run(self):
        # content, runtime and sema suites are skipped when exe(name) is missing. Fail if a test
        # binary is built under the other spelling (name or name.exe), which would skip silently.
        for name in TEST_BINARIES:
            with self.subTest(name=name):
                built = [p for p in (ROOT / 'out' / name, ROOT / 'out' / (name + '.exe')) if p.exists()]
                self.assertTrue(not built or exe(name).exists(),
                                f'{[str(p) for p in built]} built, but the tests look for {exe(name)}')


if __name__ == '__main__':
    unittest.main()
