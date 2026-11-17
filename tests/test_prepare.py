# SPDX-License-Identifier: GPL-2.0-or-later
from paths import ROOT
import contextlib
import hashlib
import io
import json
import struct
import subprocess
import sys
import unittest
import tempfile
from pathlib import Path
import prepare
from prepare import parse_self, inspect_libc, nid

TITLE = {"_license": "GPL-2.0-or-later", "schema": "pes-title-1", "title_id": "SYNT00001",
         "save_title_id": "SYNT00001", "eboot_elf_md5": "0" * 32,
         "expect": {"entry": "0x1000", "relocations_written": 5, "imports": 2},
         "gnm_region": None, "import_suffix_map": [], "import_aliases": []}


def synthetic_elf(extra_relocs=(), second_import=b'Fake0000002#A#B'):
    """Plain ELF64 made of invented bytes: two loads (file offset != vaddr), PT_DYNAMIC,
    the 0x61000000 blob with strings, symbols and two relocation tables."""
    names = [b'libsynth', b'modsynth', b'Fake0000001#A#B', second_import, b'localfn', b'absfn']
    strings, where = b'', {}
    for name in names:
        where[name] = len(strings)
        strings += name + b'\0'
    syms = [(0, 0, 0, 0, 0, 0),
            (where[b'Fake0000001#A#B'], 0x12, 0, 0, 0, 0),       # import function
            (where[second_import], 0x11, 0, 0, 0, 0),       # import object
            (where[b'localfn'], 0x12, 0, 1, 0x1010, 0),          # defined function
            (where[b'absfn'], 0x11, 0, 0xfff1, 0x77, 0)]         # absolute symbol
    rela1 = [(0x1008, 8, 0x1234), (0x1010, (1 << 32) | 7, 0), (0x2000, (2 << 32) | 6, 0),
             (0x2008, (3 << 32) | 1, 4), (0x2010, (4 << 32) | 1, 0)]
    rela2 = [(0x2018, 8, 0)] + list(extra_relocs)
    blob = strings + b'\0' * (-len(strings) % 8)
    sym_off = len(blob)
    blob += b''.join(struct.pack('<IBBHQQ', *s) for s in syms)
    rela1_off = len(blob)
    blob += b''.join(struct.pack('<QQq', *r) for r in rela1)
    rela2_off = len(blob)
    blob += b''.join(struct.pack('<QQq', *r) for r in rela2)
    lib = (0 << 48) | (1 << 32) | where[b'libsynth']      # encodes as 'A'
    mod = (1 << 48) | (1 << 32) | where[b'modsynth']      # encodes as 'B'
    dyn = [(1, where[b'libsynth']), (0x61000013, lib), (0x6100000f, mod),
           (0x61000035, 0), (0x61000037, len(strings)), (0x61000039, sym_off), (0x6100003f, 24 * len(syms)),
           (0x61000029, rela1_off), (0x6100002d, 24 * len(rela1)),
           (0x6100002f, rela2_off), (0x61000031, 24 * len(rela2)), (0, 0)]
    dyn_bytes = b''.join(struct.pack('<QQ', t, v) for t, v in dyn)
    data = bytearray(0x400 + len(blob))
    struct.pack_into('<16sHHIQQQIHHHHHH', data, 0, b'\x7fELF\x02\x01\x01' + bytes(9),
                     2, 62, 1, 0x1000, 64, 0, 0, 64, 56, 4, 64, 0, 0)
    phdrs = [(1, 5, 0x200, 0x1000, 0x1000, 0x100, 0x100, 0x1000),
             (1, 6, 0x300, 0x2000, 0x2000, 0x40, 0x80, 0x1000),
             (2, 6, 0x340, 0x340, 0x340, len(dyn_bytes), len(dyn_bytes), 8),
             (0x61000000, 4, 0x400, 0x400, 0x400, len(blob), len(blob), 8)]
    for i, ph in enumerate(phdrs):
        struct.pack_into('<IIQQQQQQ', data, 64 + 56 * i, *ph)
    data[0x200:0x300] = b'\x90' * 0x100
    data[0x340:0x340 + len(dyn_bytes)] = dyn_bytes
    data[0x400:0x400 + len(blob)] = blob
    return bytes(data)


def synthetic_sfo():
    keys, values = b'TITLE\0', b'SYNT00001\0'
    entry = struct.pack('<HHIII', 0, 0x204, len(values), len(values), 0)
    return struct.pack('<4sIIII', b'\0PSF', 0x101, 36, 36 + len(keys), 1) + entry + keys + values


def fixture():
    data = bytearray(0x210)
    data[:4] = b'O\x15=\x1d'
    struct.pack_into('<H', data, 24, 1)
    struct.pack_into('<QQQQ', data, 32, 0x800, 0x200, 16, 16)
    struct.pack_into('<16sHHIQQQIHHHHHH', data, 64,
                     b'\x7fELF\x02\x01\x01\x09', 0xfe10, 62, 1, 0, 64, 0, 0, 64, 56, 1, 0, 0, 0)
    struct.pack_into('<IIQQQQQQ', data, 128, 1, 5, 0x1000, 0, 0, 16, 32, 0x1000)
    data[0x200:] = bytes(range(16))
    return data


class SelfTests(unittest.TestCase):
    def test_libc_ret_contract_is_verified_from_symbol_and_code(self):
        def libc(instruction):
            data=bytearray(0x320)
            data[:4]=b'O\x15=\x1d'
            struct.pack_into('<H',data,24,2)
            struct.pack_into('<QQQQ',data,32,0x800,0x200,16,16)
            struct.pack_into('<QQQQ',data,64,0x100800,0x220,256,256)
            struct.pack_into('<16sHHIQQQIHHHHHH',data,96,b'\x7fELF\x02\x01\x01\x09',0xfe18,62,1,0,64,0,0,64,56,3,0,0,0)
            struct.pack_into('<IIQQQQQQ',data,160,1,5,0x1000,0,0,16,16,0x1000)
            struct.pack_into('<IIQQQQQQ',data,216,0x61000000,4,0x2000,0,0,256,0,16)
            struct.pack_into('<IIQQQQQQ',data,272,2,4,0x20b0,0,0,80,80,8)
            data[0x200]=instruction
            name=b'bzQExy189ZI#C#A\0'
            data[0x220:0x220+len(name)]=name
            struct.pack_into('<IBBHQQ',data,0x260,0,0x12,0,1,0,1)
            for i,(tag,value) in enumerate([(0x61000035,0),(0x61000037,32),(0x61000039,64),(0x6100003f,24),(0,0)]):
                struct.pack_into('<QQ',data,0x2d0+16*i,tag,value)
            return data
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'libc.prx'
            path.write_bytes(libc(0xc3))
            proof=inspect_libc(path)
            self.assertTrue(proof['init_env_is_ret'])
            self.assertEqual(proof['bytes'],'c3')
            path.write_bytes(libc(0x90))
            self.assertFalse(inspect_libc(path)['init_env_is_ret'])
        self.assertEqual(nid('_init_env'),'bzQExy189ZI')

    def test_file_offset_is_not_virtual_address(self):
        elf, header, ph, segments, missing = parse_self(fixture())
        self.assertEqual(elf[0x1000:0x1010], bytes(range(16)))
        self.assertEqual(ph[0]['vaddr'], 0)
        self.assertEqual(missing, [])

    def test_truncated_payload_rejected(self):
        with self.assertRaisesRegex(ValueError, 'out of bounds'):
            parse_self(fixture()[:-1])

    def test_encrypted_or_compressed_payload_rejected(self):
        for flag in (2, 8):
            data = fixture()
            struct.pack_into('<Q', data, 32, 0x800 | flag)
            with self.assertRaisesRegex(ValueError, 'encrypted/compressed'):
                parse_self(data)

    def test_missing_required_segment_rejected(self):
        data = fixture()
        struct.pack_into('<Q', data, 32, 0)
        with self.assertRaisesRegex(ValueError, 'required segment'):
            parse_self(data)

    def test_bad_program_header_index_rejected(self):
        data = fixture()
        struct.pack_into('<Q', data, 32, 0x100800)
        with self.assertRaisesRegex(ValueError, 'segment index'):
            parse_self(data)


class PrepareTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.tmp = Path(directory.name)

    def game(self, elf):
        game = self.tmp / 'game'
        (game / 'sce_sys').mkdir(parents=True, exist_ok=True)
        (game / 'eboot.elf').write_bytes(elf)
        (game / 'sce_sys' / 'param.sfo').write_bytes(synthetic_sfo())
        return game

    def title(self, md5):
        path = self.tmp / 'title.json'
        path.write_text(json.dumps(dict(TITLE, eboot_elf_md5=md5)), encoding='utf-8')
        return path

    def run_prepare(self, game, out, title, **kwargs):
        with contextlib.redirect_stdout(io.StringIO()):
            prepare.prepare(game, out, title, resource_inventory=False, **kwargs)

    def test_two_runs_give_identical_boot_bin(self):
        elf = synthetic_elf()
        game, title = self.game(elf), self.title(hashlib.md5(elf).hexdigest())
        self.run_prepare(game, self.tmp / 'a', title)
        self.run_prepare(game, self.tmp / 'b', title)
        self.assertEqual((self.tmp / 'a' / 'boot.bin').read_bytes(),
                         (self.tmp / 'b' / 'boot.bin').read_bytes())

    def test_missing_title_fails(self):
        script = ROOT / 'scripts' / 'prepare.py'
        result = subprocess.run([sys.executable, str(script), str(self.game(synthetic_elf())),
                                 '--out', str(self.tmp / 'out'), '--no-resource-inventory'],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('--title', result.stderr)

    def test_unmapped_relocations_raise_with_count(self):
        elf = synthetic_elf(extra_relocs=[(0x9000, 8, 0), (0x9008, 8, 0)])
        out = self.tmp / 'out'
        with self.assertRaisesRegex(ValueError, r'^2 relocation\(s\) outside mapped segments'):
            self.run_prepare(self.game(elf), out, self.title(hashlib.md5(elf).hexdigest()))
        self.assertFalse((out / 'boot.bin').exists())

    def test_md5_mismatch_is_an_error_unless_allowed(self):
        elf = synthetic_elf()
        game, title, out = self.game(elf), self.title('f' * 32), self.tmp / 'out'
        with self.assertRaisesRegex(ValueError, 'does not match title config'):
            self.run_prepare(game, out, title)
        self.run_prepare(game, out, title, allow_hash_mismatch=True)
        analysis = json.loads((out / 'analysis.json').read_text(encoding='utf-8'))
        self.assertEqual(analysis['hash_check']['status'], 'mismatch')
        self.assertTrue(analysis['hash_check']['allowed_mismatch'])

    def write_title(self, md5, **changes):
        path = self.tmp / 'title.json'
        path.write_text(json.dumps(dict(TITLE, eboot_elf_md5=md5, **changes)), encoding='utf-8')
        return path

    def alias_bytes(self, elf, title, name):
        out = self.tmp / name
        self.run_prepare(self.game(elf), out, title)
        return (out / 'import-aliases.txt').read_bytes()

    def test_empty_maps_write_an_empty_alias_file(self):
        elf = synthetic_elf()
        title = self.write_title(hashlib.md5(elf).hexdigest())
        self.assertEqual(self.alias_bytes(elf, title, 'out'), b'')

    def test_suffix_row_expands_to_per_import_lines_in_table_order(self):
        elf = synthetic_elf()
        row = dict(suffix='#A#B', library='libsynth', module='modsynth', canonical='#q#q', evidence='test')
        title = self.write_title(hashlib.md5(elf).hexdigest(), import_suffix_map=[row])
        self.assertEqual(self.alias_bytes(elf, title, 'out'),
                         b'Fake0000001#A#B Fake0000001#q#q\nFake0000002#A#B Fake0000002#q#q\n')

    def test_suffix_row_must_match_decoded_library_and_module(self):
        elf = synthetic_elf()
        row = dict(suffix='#A#B', library='libother', module='modsynth', canonical='#q#q', evidence='test')
        title = self.write_title(hashlib.md5(elf).hexdigest(), import_suffix_map=[row])
        out = self.tmp / 'out'
        with self.assertRaisesRegex(ValueError, 'import Fake0000001#A#B: decoded libsynth/modsynth'):
            self.run_prepare(self.game(elf), out, title)
        self.assertFalse((out / 'import-aliases.txt').exists())

    def test_suffix_row_needs_decoded_library_and_module(self):
        elf = synthetic_elf(second_import=b'Fake0000002#C#B')
        row = dict(suffix='#C#B', library='libsynth', module='modsynth', canonical='#q#q', evidence='test')
        title = self.write_title(hashlib.md5(elf).hexdigest(), import_suffix_map=[row])
        with self.assertRaisesRegex(ValueError, 'Fake0000002#C#B: library/module not decoded'):
            self.run_prepare(self.game(elf), self.tmp / 'out', title)

    def test_explicit_alias_adds_its_import_line(self):
        elf = synthetic_elf()
        row = {'import': 'Fake0000002#A#B', 'canonical': 'Fake0000002#q#x', 'reason': 'test', 'evidence': 'test'}
        title = self.write_title(hashlib.md5(elf).hexdigest(), import_aliases=[row])
        self.assertEqual(self.alias_bytes(elf, title, 'out'), b'Fake0000002#A#B Fake0000002#q#x\n')

    def test_explicit_alias_must_name_an_existing_import(self):
        elf = synthetic_elf()
        row = {'import': 'Nope0000000#A#B', 'canonical': 'Nope0000000#q#q', 'reason': 'test', 'evidence': 'test'}
        title = self.write_title(hashlib.md5(elf).hexdigest(), import_aliases=[row])
        with self.assertRaisesRegex(ValueError, r"import_aliases\[0\]: import 'Nope0000000#A#B' is not in this eboot"):
            self.run_prepare(self.game(elf), self.tmp / 'out', title)

    def test_explicit_alias_canonical_must_be_scoped(self):
        elf = synthetic_elf()
        row = {'import': 'Fake0000002#A#B', 'canonical': 'Fake0000002#q', 'reason': 'test', 'evidence': 'test'}
        title = self.write_title(hashlib.md5(elf).hexdigest(), import_aliases=[row])
        with self.assertRaisesRegex(ValueError, r'import_aliases\[0\]\.canonical: must be NID#library#module'):
            self.run_prepare(self.game(elf), self.tmp / 'out', title)

    def test_explicit_alias_cannot_repeat_a_suffix_row(self):
        elf = synthetic_elf()
        suffix = dict(suffix='#A#B', library='libsynth', module='modsynth', canonical='#q#q', evidence='test')
        row = {'import': 'Fake0000002#A#B', 'canonical': 'Fake0000002#q#x', 'reason': 'test', 'evidence': 'test'}
        title = self.write_title(hashlib.md5(elf).hexdigest(), import_suffix_map=[suffix], import_aliases=[row])
        with self.assertRaisesRegex(ValueError, 'already covered by import_suffix_map'):
            self.run_prepare(self.game(elf), self.tmp / 'out', title)

    def test_analysis_records_required_fields(self):
        elf = synthetic_elf()
        game, title, out = self.game(elf), self.title(hashlib.md5(elf).hexdigest()), self.tmp / 'out'
        self.run_prepare(game, out, title)
        analysis = json.loads((out / 'analysis.json').read_text(encoding='utf-8'))
        for key in ('input_sha256', 'input_md5', 'image_sha256', 'relocation_records_total',
                    'relocations_written', 'import_identities', 'title_config', 'libc_evidence',
                    'hash_check'):
            self.assertIn(key, analysis)
        self.assertEqual(analysis['input_sha256'], hashlib.sha256(elf).hexdigest())
        self.assertEqual(analysis['input_md5'], hashlib.md5(elf).hexdigest())
        self.assertEqual(analysis['hash_check']['status'], 'match')
        self.assertEqual(analysis['relocation_records_total'], 6)
        self.assertEqual(analysis['relocations_written'], 5)
        self.assertEqual(analysis['import_identities'], {
            'Fake0000001#A#B': ['Fake0000001', 'libsynth', 'modsynth'],
            'Fake0000002#A#B': ['Fake0000002', 'libsynth', 'modsynth']})
        self.assertEqual(analysis['title_config']['sha256'], hashlib.sha256(title.read_bytes()).hexdigest())
        self.assertEqual(analysis['libc_evidence'], {'status': 'absent', 'init_env_is_ret': False})
        raw = (out / 'boot.bin').read_bytes()
        magic, size, entry, _, relocs, _, capability = struct.unpack_from('<8sQQQQQQ', raw, 0)
        self.assertEqual((magic, entry, relocs, capability), (b'PESIMG1\0', 0x1000, 5, 0))
        self.assertEqual(analysis['image_sha256'], hashlib.sha256(raw[-size:]).hexdigest())


if __name__ == '__main__':
    unittest.main()
