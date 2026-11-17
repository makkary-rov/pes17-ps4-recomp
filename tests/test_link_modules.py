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

    def test_plain_elf_input_is_accepted(self):
        text = (ROOT / 'scripts' / 'link_modules.py').read_text(encoding='utf-8')
        self.assertIn("parse_elf(source)", text)
        self.assertIn("'eboot.elf'", text)


if __name__ == '__main__':
    unittest.main()
