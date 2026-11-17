# SPDX-License-Identifier: GPL-2.0-or-later
"""link_modules.py must import cleanly and must not depend on removed scripts."""
from paths import ROOT
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

TITLE = ROOT / 'src' / 'title' / 'CUSA04957.json'


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
        import link_modules

        class Routed(Exception):
            pass

        def which(name):
            def fake(data):
                raise Routed(name)
            return fake
        with tempfile.TemporaryDirectory() as tmp:
            elf = Path(tmp) / 'plain.elf'
            elf.write_bytes(b'\x7fELF' + bytes(60))
            sce = Path(tmp) / 'self.bin'
            sce.write_bytes(b'O\x15=\x1d' + bytes(60))
            with mock.patch.object(link_modules, 'parse_elf', which('elf')), \
                    mock.patch.object(link_modules, 'parse_self', which('self')):
                with self.assertRaises(Routed) as e:
                    link_modules.module(elf)
                self.assertEqual(str(e.exception), 'elf')
                with self.assertRaises(Routed) as e:
                    link_modules.module(sce)
                self.assertEqual(str(e.exception), 'self')

    def test_eboot_elf_is_preferred_input(self):
        text = (ROOT / 'scripts' / 'link_modules.py').read_text(encoding='utf-8')
        self.assertIn("'eboot.elf'", text)

    def test_linker_aliases_added_imports_with_suffix_map(self):
        import link_modules
        config = json.loads(TITLE.read_text(encoding='utf-8'))
        row = config['import_suffix_map'][0]
        good = 'AAAAAAAAAAA' + row['suffix']
        other = 'CCCCCCCCCCC#9#9'
        added = {good: ['AAAAAAAAAAA', row['library'], row['module']],
                 other: ['CCCCCCCCCCC', 'libUnmapped', 'libUnmapped']}
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            (out / 'import-aliases.txt').write_text('existing#x#y canonical#q#q\n', encoding='utf-8')
            count = link_modules.linked_aliases(added, TITLE, out)
            text = (out / 'import-aliases.txt').read_text(encoding='utf-8')
            self.assertEqual(count, 1)
            self.assertIn('existing#x#y canonical#q#q\n', text)
            self.assertIn(good + ' AAAAAAAAAAA' + row['canonical'] + '\n', text)
            self.assertNotIn('CCCCCCCCCCC', text)
            # a second run adds nothing new (idempotent)
            link_modules.linked_aliases(added, TITLE, out)
            self.assertEqual(text, (out / 'import-aliases.txt').read_text(encoding='utf-8'))
            # a library/module mismatch is an error, never a silent alias
            bad = {good: ['AAAAAAAAAAA', 'libWrong', row['module']]}
            with self.assertRaises(ValueError):
                link_modules.linked_aliases(bad, TITLE, out)


if __name__ == '__main__':
    unittest.main()
