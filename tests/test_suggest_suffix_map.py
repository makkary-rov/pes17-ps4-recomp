# SPDX-License-Identifier: GPL-2.0-or-later
import csv
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from paths import ROOT
import suggest_suffix_map as sm

SCRIPT = ROOT / 'scripts' / 'suggest_suffix_map.py'
HEADER = ['index', 'name', 'kind', 'looked_up_as', 'result', 'symbol', 'prefix_candidate']


def row(index, name, result='trap', candidate='-'):
    return {'index': str(index), 'name': name, 'kind': 'func', 'looked_up_as': name,
            'result': result, 'symbol': '-', 'prefix_candidate': candidate}


# Synthetic fixture: all names and identities are invented for this test.
AGREE = [  # suffix #A#B: two prefix_only imports, same candidate suffix, same decoded identity
    row(0, 'nidAAAAAAAA#A#B', candidate='nidAAAAAAAA#x#y'),
    row(1, 'nidBBBBBBBB#A#B', candidate='nidBBBBBBBB#x#y'),
    row(2, 'nidCCCCCCCC#A#B', result='trap'),  # same suffix, no candidate: not prefix_only
]
AGREE_ID = {'nidAAAAAAAA#A#B': ['nidAAAAAAAA', 'libfix', 'modfix'],
            'nidBBBBBBBB#A#B': ['nidBBBBBBBB', 'libfix', 'modfix'],
            'nidCCCCCCCC#A#B': ['nidCCCCCCCC', 'libfix', 'modfix']}

DISAGREE = [  # suffix #C#D: candidates disagree, so no suffix row; each import keeps its candidate
    row(3, 'nidDDDDDDDD#C#D', candidate='nidDDDDDDDD#p#J'),
    row(4, 'nidEEEEEEEE#C#D', candidate='nidEEEEEEEE#q#q'),
]
DISAGREE_ID = {'nidDDDDDDDD#C#D': ['nidDDDDDDDD', 'libdis', 'moddis'],
               'nidEEEEEEEE#C#D': ['nidEEEEEEEE', 'libdis', 'moddis']}

NULL_LIB = [  # suffix #E#F: library not decoded for one import, so no suffix row
    row(5, 'nidFFFFFFFF#E#F', candidate='nidFFFFFFFF#x#y'),
    row(6, 'nidGGGGGGGG#E#F', candidate='nidGGGGGGGG#x#y'),
]
NULL_LIB_ID = {'nidFFFFFFFF#E#F': ['nidFFFFFFFF', 'libnul', 'modnul'],
               'nidGGGGGGGG#E#F': ['nidGGGGGGGG', None, 'modnul']}

RESOLVED = [  # suffix #G#H: one import resolves by name, so a suffix row would rewrite it
    row(7, 'nidHHHHHHHH#G#H', candidate='nidHHHHHHHH#x#y'),
    row(8, 'nidIIIIIIII#G#H', result='host'),
]
RESOLVED_ID = {'nidHHHHHHHH#G#H': ['nidHHHHHHHH', 'libres', 'modres'],
               'nidIIIIIIII#G#H': ['nidIIIIIIII', 'libres', 'modres']}

IDENTITIES = {**AGREE_ID, **DISAGREE_ID, **NULL_LIB_ID, **RESOLVED_ID}
ROWS = AGREE + DISAGREE + NULL_LIB + RESOLVED


class SuggestTests(unittest.TestCase):
    def test_agreeing_group_becomes_suffix_row(self):
        suffix_rows, alias_rows, conflicts, summary = sm.suggest(AGREE, AGREE_ID)
        self.assertEqual(suffix_rows, [{'suffix': '#A#B', 'library': 'libfix', 'module': 'modfix',
                                        'canonical': '#x#y', 'evidence': 'same-hash rows: 2'}])
        self.assertEqual(alias_rows, [])
        self.assertEqual(conflicts, [])
        self.assertEqual(summary['prefix_only_covered'], 2)
        self.assertEqual(summary['prefix_only_uncovered'], 0)

    def test_disagreeing_group_becomes_conflict_with_per_import_aliases(self):
        suffix_rows, alias_rows, conflicts, summary = sm.suggest(DISAGREE, DISAGREE_ID)
        self.assertEqual(suffix_rows, [])
        self.assertEqual(len(conflicts), 1)
        self.assertEqual(conflicts[0]['suffix'], '#C#D')
        self.assertIn('prefix candidates disagree: #p#J, #q#q', conflicts[0]['reasons'])
        self.assertEqual([a['import'] for a in alias_rows], ['nidDDDDDDDD#C#D', 'nidEEEEEEEE#C#D'])
        self.assertEqual([a['canonical'] for a in alias_rows], ['nidDDDDDDDD#p#J', 'nidEEEEEEEE#q#q'])
        self.assertEqual(summary['prefix_only_covered'], 2)

    def test_null_library_becomes_conflict(self):
        suffix_rows, alias_rows, conflicts, _ = sm.suggest(NULL_LIB, NULL_LIB_ID)
        self.assertEqual(suffix_rows, [])
        self.assertEqual(len(conflicts), 1)
        self.assertIn('library or module not decoded for some import with this suffix',
                      conflicts[0]['reasons'])
        self.assertEqual(len(alias_rows), 2)

    def test_group_with_resolved_import_becomes_conflict(self):
        suffix_rows, alias_rows, conflicts, _ = sm.suggest(RESOLVED, RESOLVED_ID)
        self.assertEqual(suffix_rows, [])
        self.assertEqual(conflicts[0]['reasons'], ['imports already resolve by name: 1'])
        self.assertEqual([a['import'] for a in alias_rows], ['nidHHHHHHHH#G#H'])

    def test_suffix_without_prefix_only_is_skipped(self):
        rows = [row(9, 'nidJJJJJJJJ#K#L'), row(10, 'nidKKKKKKKK#K#L')]
        self.assertEqual(sm.suggest(rows, {})[:3], ([], [], []))

    def test_output_is_deterministic_for_input_order(self):
        forward = sm.suggest(ROWS, IDENTITIES)
        backward = sm.suggest(list(reversed(ROWS)), IDENTITIES)
        self.assertEqual(forward, backward)

    def test_non_three_part_names_are_ignored(self):
        self.assertIsNone(sm.suffix_of('plainname'))
        self.assertIsNone(sm.suffix_of('a#b#c#d'))
        self.assertEqual(sm.suffix_of('nid#1#2'), '#1#2')


class CliTests(unittest.TestCase):
    def test_cli_writes_json_from_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            analysis = tmp / 'analysis.json'
            analysis.write_text(json.dumps({'import_identities': IDENTITIES}), encoding='utf-8')
            tsv = tmp / 'imports.tsv'
            with open(tsv, 'w', encoding='utf-8', newline='') as handle:
                writer = csv.writer(handle, delimiter='\t', lineterminator='\n')
                writer.writerow(HEADER)
                for r in ROWS:
                    writer.writerow([r[h] for h in HEADER])
            out = tmp / 'out.json'
            res = subprocess.run([sys.executable, str(SCRIPT), '--analysis', str(analysis),
                                  '--imports', str(tsv), '--out', str(out)],
                                 stdin=subprocess.DEVNULL, capture_output=True, text=True)
            self.assertEqual(res.returncode, 0, res.stderr)
            data = json.loads(out.read_text(encoding='utf-8'))
            self.assertEqual([r['suffix'] for r in data['import_suffix_map']], ['#A#B'])
            self.assertEqual([c['suffix'] for c in data['conflicts']], ['#C#D', '#E#F', '#G#H'])
            self.assertEqual(data['summary']['prefix_only_total'], 7)


if __name__ == '__main__':
    unittest.main()
