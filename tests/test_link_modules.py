# SPDX-License-Identifier: GPL-2.0-or-later
"""link_modules.py must import cleanly and must not depend on removed scripts."""
from paths import ROOT
import unittest


class LinkModulesTests(unittest.TestCase):
    def test_imports_without_removed_modules(self):
        import link_modules
        self.assertTrue(callable(link_modules.link))
        self.assertTrue(callable(link_modules.module))

    def test_no_reference_to_removed_scripts(self):
        text = (ROOT / 'scripts' / 'link_modules.py').read_text(encoding='utf-8')
        for name in ('redzone', 'engine_assets', 'protect_image', 'EBOOT_SHA'):
            self.assertNotIn(name, text)

    def test_red_zone_is_reported_disabled(self):
        text = (ROOT / 'scripts' / 'link_modules.py').read_text(encoding='utf-8')
        self.assertIn("red_zone = dict(enabled=False", text)

    def test_plain_elf_input_is_routed_to_parse_elf(self):
        import tempfile
        from pathlib import Path
        from unittest import mock
        import link_modules

        class Routed(Exception):
            pass

        def which(name):
            def fake(data):
                raise Routed(name)
            return fake
        with tempfile.TemporaryDirectory() as tmp:
            elf = Path(tmp) / 'plain.elf'
            elf.write_bytes(b'ELF' + bytes(60))
            sce = Path(tmp) / 'self.bin'
            sce.write_bytes(b'O=' + bytes(60))
            with mock.patch.object(link_modules, 'parse_elf', which('elf')),                     mock.patch.object(link_modules, 'parse_self', which('self')):
                with self.assertRaises(Routed) as e:
                    link_modules.module(elf)
                self.assertEqual(str(e.exception), 'elf')
                with self.assertRaises(Routed) as e:
                    link_modules.module(sce)
                self.assertEqual(str(e.exception), 'self')

    def test_eboot_elf_is_preferred_input(self):
        text = (ROOT / 'scripts' / 'link_modules.py').read_text(encoding='utf-8')
        self.assertIn("'eboot.elf'", text)


if __name__ == '__main__':
    unittest.main()
