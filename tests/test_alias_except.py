# SPDX-License-Identifier: GPL-2.0-or-later
"""Suffix rows may leave named imports out ("except"): they keep resolving by their own name."""
from paths import ROOT
import json
import tempfile
import unittest
from pathlib import Path

import link_modules
import prepare
import title_config

ROW = dict(suffix='#A#B', library='libsynth', module='modsynth', canonical='#q#q', evidence='test',
           **{'except': ['Keep0000001#A#B']})
CONFIG = dict(import_suffix_map=[ROW], import_aliases=[])
NAMES = ['Take0000001#A#B', 'Keep0000001#A#B', 'Take0000002#A#B']
IDENT = {n: [n.split('#')[0], 'libsynth', 'modsynth'] for n in NAMES}


class ExceptTests(unittest.TestCase):
    def test_prepare_skips_excepted_imports(self):
        text = prepare.build_import_aliases(NAMES, IDENT, CONFIG)
        self.assertEqual(text.splitlines(),
                         ['Take0000001#A#B Take0000001#q#q', 'Take0000002#A#B Take0000002#q#q'])

    def test_linker_skips_excepted_imports(self):
        added = {n: IDENT[n] for n in NAMES}
        with tempfile.TemporaryDirectory() as tmp:
            cfg = Path(tmp) / 'title.json'
            real = json.loads((ROOT / 'src' / 'title' / 'CUSA04957.json').read_text(encoding='utf-8'))
            real['import_suffix_map'] = [ROW]
            real['import_aliases'] = []
            cfg.write_text(json.dumps(real), encoding='utf-8')
            out = Path(tmp)
            count = link_modules.linked_aliases(added, cfg, out)
            text = (out / 'import-aliases.txt').read_text(encoding='utf-8')
            self.assertEqual(count, 2)
            self.assertNotIn('Keep0000001', text)

    def test_validator_accepts_and_checks_except(self):
        real = json.loads((ROOT / 'src' / 'title' / 'CUSA04957.json').read_text(encoding='utf-8'))
        real['import_suffix_map'] = [ROW]
        real['import_aliases'] = []
        title_config.validate(real)
        for bad in (['Keep0000001#X#Y'], ['Keep0000001#A#B', 'Keep0000001#A#B'], 'Keep0000001#A#B', [5]):
            broken = json.loads(json.dumps(real))
            broken['import_suffix_map'][0]['except'] = bad
            with self.assertRaises(title_config.TitleConfigError):
                title_config.validate(broken)

    def test_shipped_config_excepts_the_names_that_resolve_exactly(self):
        real = json.loads((ROOT / 'src' / 'title' / 'CUSA04957.json').read_text(encoding='utf-8'))
        row = next(r for r in real['import_suffix_map'] if r['suffix'] == '#3#D')
        self.assertEqual((row['library'], row['module'], row['canonical']), ('libkernel', 'libkernel', '#p#J'))
        self.assertEqual(len(row['except']), 3)


if __name__ == '__main__':
    unittest.main()
