# SPDX-License-Identifier: GPL-2.0-or-later
"""Boundary tests for the native loader; uses tiny synthetic x86-64 images."""
from paths import ROOT
from pathlib import Path
import itertools
import os
import struct
import subprocess
import sys
import tempfile
import unittest

# The MSYS2 CLANG64 build (Windows) writes out/pes17recomp.exe; POSIX writes out/pes17recomp.
WINDOWS = os.name == 'nt' or sys.platform in ('cygwin', 'msys')
EXE = ROOT / ('out/pes17recomp.exe' if WINDOWS else 'out/pes17recomp')


def package(code, relocs=(), names=(), capabilities=None):
    image = code.ljust(4096, b'\0')
    header = struct.pack('<8sQQQQQ', b'PESIMG0\0' if capabilities is None else b'PESIMG1\0', len(image), 0, 1, len(relocs), len(names))
    if capabilities is not None:
        header += struct.pack('<Q', capabilities)
    segment = struct.pack('<QQQ', 0, len(image), 5)
    return (header + segment + b''.join(n.encode().ljust(128, b'\0') for n in names)
            + b''.join(struct.pack('<QQqq', *r) for r in relocs) + image)


def native_package(name='fixture-native', binding_address=4112, binding_kind=1,
                   lib_flags=5, init=b'\x31\xc0\xc3', native=b'\xc3', metadata=None):
    # Main calls an import then tail-jumps to the terminal diagnostic import.
    code = b'\x48\x83\xec\x08\xff\x15\x16\0\0\0\x48\x83\xc4\x08\xff\x25\x14\0\0\0'
    image = code.ljust(4096,b'\0') + init.ljust(16,b'\0') + native
    image = image.ljust(12288,b'\0')
    header=struct.pack('<8s6Q',b'PESIMG3\0',len(image),0,3,2,2,1)
    meta=metadata or (4096,8192,4096,8192,32,4,1,256)
    return (header + struct.pack('<8Q',*meta) + struct.pack('<3Q',0,binding_address,binding_kind)
            + struct.pack('<9Q',0,4096,5,4096,4096,lib_flags,8192,4096,6)
            + name.encode().ljust(128,b'\0') + b'after-native'.ljust(128,b'\0')
            + struct.pack('<8Q',32,1,0,0,40,1,1,0) + image)


def sfo_bytes(entries):
    # PS4 param.sfo with string (0x0204) values: header, index table, key table, value table.
    keys, values, index = b'', b'', b''
    for key, value in entries.items():
        data = value.encode() + b'\0'
        index += struct.pack('<HHIII', len(keys), 0x0204, len(data), len(data), len(values))
        keys += key.encode() + b'\0'
        values += data
    keys_offset = 20 + len(index)
    return struct.pack('<4sIIII', b'\0PSF', 0x101, keys_offset, keys_offset + len(keys), len(entries)) + index + keys + values


REPORT_IMAGE_SIZE = 12288

def report_package(code):
    # Three imports: 0 host (_init_env), 1 native (export at image +0x1010, module init returns 0),
    # 2 trap whose NID matches _init_env with another suffix (no alias row). Slots at +0x100/+0x108/+0x110.
    image = (code.ljust(4096, b'\0') + b'\x31\xc0\xc3'.ljust(16, b'\0') + b'\xc3').ljust(REPORT_IMAGE_SIZE, b'\0')
    names = ['bzQExy189ZI#q#q', 'fixture-native', 'bzQExy189ZI#p#J']
    header = struct.pack('<8s6Q', b'PESIMG3\0', len(image), 0, 3, 3, 3, 1)
    module = struct.pack('<8Q', 4096, 8192, 4096, 8192, 32, 4, 1, 256)
    binding = struct.pack('<3Q', 1, 4112, 1)
    segments = struct.pack('<9Q', 0, 4096, 5, 4096, 4096, 5, 8192, 4096, 6)
    relocs = b''.join(struct.pack('<4Q', target, 1, index, 0) for index, target in enumerate((0x100, 0x108, 0x110)))
    return (header + module + binding + segments + b''.join(n.encode().ljust(128, b'\0') for n in names)
            + relocs + image)


@unittest.skipUnless(EXE.exists(), f'loader not built: {EXE} is missing (run ./build.sh first)')
class LoaderTests(unittest.TestCase):
    def test_invalid_content_profile_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'content.bin'
            for data in (b'PESCONT1',struct.pack('<8s5I',b'PESCONT1',2,0,0,0,0),
                         struct.pack('<8s5I',b'PESCONT1',3,0,0,0,0)+b'extra'):
                with self.subTest(data=data):
                    path.write_bytes(data)
                    r=self.run_image(package(b'\xc3'),'--content-profile',str(path))
                    self.assertEqual(r.returncode,1,r.stdout+r.stderr)
                    self.assertIn('content profile',r.stderr)

    def run_image(self, data, *options):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'boot.bin'
            path.write_bytes(data)
            return subprocess.run([str(EXE.resolve()), str(path), '--cpu-only', *options],
                                  stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=5)

    def test_original_instruction_reaches_named_import(self):
        # jmp [rip+2]; two padding bytes; relocated function pointer at +8
        r = self.run_image(package(b'\xff\x25\x02\0\0\0\x90\x90', [(8, 1, 0, 0)], ['fixture-import']))
        self.assertEqual(r.returncode, 20, r.stderr)
        self.assertIn('first unsupported PS4 import: fixture-import', r.stdout)

    def test_native_initializer_and_export_return(self):
        r=self.run_image(native_package())
        self.assertEqual(r.returncode,20,r.stdout+r.stderr)
        self.assertIn('Module 0 initializer returned 0',r.stdout)
        self.assertIn('first unsupported PS4 import: after-native',r.stdout)

    def test_host_contract_takes_priority_over_native_export(self):
        r=self.run_image(native_package(name='bzQExy189ZI#q#q',native=b'\x0f\x0b'))
        self.assertEqual(r.returncode,20,r.stdout+r.stderr)
        self.assertIn('_init_env returned',r.stdout)
        self.assertIn('first unsupported PS4 import: after-native',r.stdout)

    def test_strict_mode_skips_native_initialization_and_binding(self):
        r=self.run_image(native_package(init=b'\x0f\x0b'), '--strict-imports')
        self.assertEqual(r.returncode,20,r.stdout+r.stderr)
        self.assertIn('first unsupported PS4 import: fixture-native',r.stdout)
        self.assertNotIn('Starting native libc',r.stdout)

    def test_native_metadata_and_bindings_are_validated(self):
        for kwargs,message in [
            ({'binding_address':12288},'invalid native binding'),
            ({'binding_kind':2},'native export kind/range mismatch'),
            ({'lib_flags':4},'native function is not executable'),
            ({'metadata':(4096,8192,4096,8192,32,33,1,256)},'invalid linked module metadata'),
            ({'metadata':(4096,8192,4096,8192,32,4,1,12280)},'unmapped procparam'),
        ]:
            with self.subTest(kwargs=kwargs):
                r=self.run_image(native_package(**kwargs))
                self.assertEqual(r.returncode,1,r.stdout+r.stderr)
                self.assertIn(message,r.stderr)

    def test_failed_native_initializer_does_not_enter_game(self):
        r=self.run_image(native_package(init=b'\xb8\x01\0\0\0\xc3'))
        self.assertEqual(r.returncode,1,r.stdout+r.stderr)
        self.assertIn('module initializer failed',r.stderr)
        self.assertNotIn('Entering original',r.stdout)

    def exit_image(self, name, *unused_imports):
        # mov edi,7; jmp [rip+5] through a relocated import pointer at +16 (import 0 is `name`)
        code = b'\xbf\x07\0\0\0\xff\x25\x05\0\0\0\x90\x90\x90\x90\x90' + b'\0' * 8
        return package(code, [(16, 1, 0, 0)], [name, *unused_imports], capabilities=1)

    def run_aliased(self, data, text):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'import-aliases.txt'
            path.write_bytes(text.encode())
            return self.run_image(data, '--import-aliases', str(path))

    def test_import_alias_maps_full_name_to_canonical(self):
        r = self.run_aliased(self.exit_image('fixture-import'), 'fixture-import uMei1W9uyNo#q#q\n')
        self.assertEqual(r.returncode, 7, r.stdout + r.stderr)
        self.assertIn('guest requested exit(7)', r.stdout)

    def test_unaliased_import_still_traps(self):
        r = self.run_image(self.exit_image('fixture-import'))
        self.assertEqual(r.returncode, 20, r.stdout + r.stderr)
        self.assertIn('first unsupported PS4 import: fixture-import', r.stdout)

    def test_same_nid_with_other_suffix_is_not_matched(self):
        r = self.run_image(self.exit_image('uMei1W9uyNo#q#x'))
        self.assertEqual(r.returncode, 20, r.stdout + r.stderr)
        self.assertIn('first unsupported PS4 import: uMei1W9uyNo#q#x', r.stdout)
        r = self.run_aliased(self.exit_image('uMei1W9uyNo#q#x'), 'fixture-import uMei1W9uyNo#q#q\n')
        self.assertEqual(r.returncode, 20, r.stdout + r.stderr)
        self.assertIn('first unsupported PS4 import: uMei1W9uyNo#q#x', r.stdout)

    def test_alias_lookup_is_exact_on_the_import_side(self):
        # A boot.bin import that is a prefix or an extension of an alias row's import name stays unresolved.
        r = self.run_aliased(self.exit_image('fixture-import-x'), 'fixture-import uMei1W9uyNo#q#q\n')
        self.assertEqual(r.returncode, 20, r.stdout + r.stderr)
        self.assertIn('first unsupported PS4 import: fixture-import-x', r.stdout)
        r = self.run_aliased(self.exit_image('fixture-import'), 'fixture-import-x uMei1W9uyNo#q#q\n')
        self.assertEqual(r.returncode, 20, r.stdout + r.stderr)
        self.assertIn('first unsupported PS4 import: fixture-import', r.stdout)
        # With a row for the longer name as well (the other import is unused), each import still
        # resolves only through its own row.
        both = 'fixture-import uMei1W9uyNo#q#q\nfixture-import-x uMei1W9uyNo#q#q\n'
        r = self.run_aliased(self.exit_image('fixture-import', 'fixture-import-x'), both)
        self.assertEqual(r.returncode, 7, r.stdout + r.stderr)
        r = self.run_aliased(self.exit_image('fixture-import-x', 'fixture-import'), both)
        self.assertEqual(r.returncode, 7, r.stdout + r.stderr)
        r = self.run_aliased(self.exit_image('fixture-import-xy', 'fixture-import'), both)
        self.assertEqual(r.returncode, 20, r.stdout + r.stderr)
        self.assertIn('first unsupported PS4 import: fixture-import-xy', r.stdout)

    def test_alias_names_of_128_bytes_or_more_are_rejected(self):
        # boot.bin name fields are 128 bytes with a NUL, so a 127-byte name is the longest usable one.
        canonical_tail = 'uMei1W9uyNo#q#' + 'q' * (128 - len('uMei1W9uyNo#q#'))
        for text in ('a' * 128 + ' uMei1W9uyNo#q#q\n', 'fixture-import ' + canonical_tail + '\n'):
            with self.subTest(text=text[:40]):
                r = self.run_aliased(self.exit_image('fixture-import'), text)
                self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
                self.assertIn('ERROR: import aliases line 1: name too long', r.stderr)

    def test_alias_name_of_127_bytes_is_accepted(self):
        name = 'n' * 127
        r = self.run_aliased(self.exit_image(name), f'{name} uMei1W9uyNo#q#q\n')
        self.assertEqual(r.returncode, 7, r.stdout + r.stderr)

    def test_alias_file_comments_blank_lines_and_crlf_are_accepted(self):
        r = self.run_aliased(self.exit_image('fixture-import'),
                             '# generated\r\n\r\nfixture-import uMei1W9uyNo#q#q\r\n')
        self.assertEqual(r.returncode, 7, r.stdout + r.stderr)

    def test_malformed_alias_lines_are_rejected(self):
        cases = [('fixture-import\n', "expected '<import> <canonical>'"),
                 ('fixture-import uMei1W9uyNo#q#q extra\n', 'expected exactly two names'),
                 ('fixture-import uMei1W9uyNo\n', 'canonical name must be NID#library#module'),
                 ('fixture-import uMei1W9uyNo#q#q\nfixture-import uMei1W9uyNo#q#q\n', 'duplicate import alias'),
                 ('fixture-import uMei1W9uyNo#q#q\nother uMei1W9uyNo#q#q\n', 'more alias lines than imports')]
        for text, message in cases:
            with self.subTest(text=text):
                r = self.run_aliased(self.exit_image('fixture-import'), text)
                self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
                self.assertIn('ERROR: import aliases line', r.stderr)
                self.assertIn(message, r.stderr)

    def test_missing_alias_file_is_an_error(self):
        r = self.run_image(self.exit_image('fixture-import'), '--import-aliases', 'no-such-aliases.txt')
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('cannot open import aliases', r.stderr)

    def test_base_relative_address_is_relocated(self):
        # jump via a relative relocation to code at +16, then to an import at +24
        code = b'\xff\x25\x02\0\0\0\x90\x90' + b'\0'*8 + b'\xff\x25\x02\0\0\0\x90\x90'
        r = self.run_image(package(code, [(8, 0, 16, 0), (24, 1, 0, 0)], ['after-relative-jump']))
        self.assertEqual(r.returncode, 20, r.stderr)
        self.assertIn('after-relative-jump', r.stdout)

    def test_out_of_bounds_relocation_is_rejected(self):
        r = self.run_image(package(b'\xc3', [(4092, 0, 0, 0)]))
        self.assertEqual(r.returncode, 1)
        self.assertIn('bad relocation', r.stderr)

    def test_truncated_image_is_rejected(self):
        r = self.run_image(package(b'\xc3')[:-1])
        self.assertEqual(r.returncode, 1)
        self.assertIn('incorrect memory image size', r.stderr)

    def test_illegal_instruction_reports_guest_offset(self):
        r = self.run_image(package(b'\x0f\x0b'))
        if WINDOWS:
            # Measured on the CLANG64 build: the fault arrives as STATUS_ILLEGAL_INSTRUCTION (0xc000001d),
            # the loader exits 139 (terminate(128 + 11)), and stderr says "rip ... guest offset 0x0" with
            # no comma (src/win32_crash.c). The POSIX expectation below is unchanged.
            self.assertEqual(r.returncode, 139, r.stdout + r.stderr)
            self.assertIn('guest offset 0x0', r.stderr)
        else:
            self.assertEqual(r.returncode, 132)
            self.assertIn('at guest offset 0x0,', r.stderr)

    def test_runtime_returns_from_verified_init_env(self):
        # Align stack, call _init_env through +32, then tail-jump to unknown +40.
        code = b'\x48\x83\xec\x08\xff\x15\x16\0\0\0\x48\x83\xc4\x08\xff\x25\x14\0\0\0'
        data = package(code, [(32,1,0,0),(40,1,1,0)], ['bzQExy189ZI#q#q','after-init'], 1)
        r = self.run_image(data)
        self.assertEqual(r.returncode,20,r.stderr)
        self.assertIn('first unsupported PS4 import: after-init',r.stdout)
        self.assertIn('_init_env=1',r.stdout)
        strict = self.run_image(data,'--strict-imports')
        self.assertEqual(strict.returncode,20,strict.stderr)
        self.assertIn('first unsupported PS4 import: bzQExy189ZI#q#q',strict.stdout)

    def test_unverified_runtime_stays_disabled(self):
        r = self.run_image(package(b'\xff\x25\x02\0\0\0\x90\x90',[(8,1,0,0)],['bzQExy189ZI#q#q'],0))
        self.assertEqual(r.returncode,20,r.stderr)
        self.assertIn('_init_env=0',r.stdout)

    def test_unknown_capabilities_rejected(self):
        r = self.run_image(package(b'\xc3',capabilities=2))
        self.assertEqual(r.returncode,1)
        self.assertIn('unknown runtime capabilities',r.stderr)

    def test_import_report_classifies_host_native_and_trap_rows(self):
        with tempfile.TemporaryDirectory() as tmp:
            report = Path(tmp) / 'imports.tsv'
            r = self.run_image(report_package(b'\x0f\x0b'), '--stop-before-entry', '--import-report', str(report))
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            rows = report.read_bytes().decode('ascii').splitlines()
        self.assertEqual(rows, [
            'index\tname\tkind\tlooked_up_as\tresult\tsymbol\tprefix_candidate',
            '0\tbzQExy189ZI#q#q\tfunc\tbzQExy189ZI#q#q\thost\t_init_env\t-',
            '1\tfixture-native\tfunc\tfixture-native\tnative\t-\t-',
            '2\tbzQExy189ZI#p#J\tfunc\tbzQExy189ZI#p#J\ttrap\t-\tbzQExy189ZI#q#q',
        ])
        # The summary is printed when the report is written, before native module initializers run.
        lines = r.stdout.splitlines()
        self.assertIn('Imports: 1 host, 1 native, 1 trap, 1 prefix_only', lines)
        self.assertEqual(lines[-1], 'Entering original x86-64 code at guest offset 0x0')

    def test_trap_import_with_prefix_candidate_still_traps(self):
        with tempfile.TemporaryDirectory() as tmp:
            report = Path(tmp) / 'imports.tsv'
            # sub rsp,8 (SysV alignment at the trap); call [rip+0x110]: the slot holding trap import 2
            code = b'\x48\x83\xec\x08\xff\x15' + struct.pack('<i', 0x110 - 10)
            r = self.run_image(report_package(code), '--import-report', str(report))
            self.assertEqual(r.returncode, 20, r.stdout + r.stderr)
            self.assertIn('first unsupported PS4 import: bzQExy189ZI#p#J', r.stdout)
            self.assertIn('API: name not resolved', r.stdout)
            self.assertIn('2\tbzQExy189ZI#p#J\tfunc\tbzQExy189ZI#p#J\ttrap\t-\tbzQExy189ZI#q#q',
                          report.read_bytes().decode('ascii'))
            self.assertIn('Imports: 1 host, 1 native, 1 trap, 1 prefix_only', r.stdout)

    def test_dump_image_writes_the_bytes_before_relocation(self):
        data = report_package(b'\x0f\x0b')
        with tempfile.TemporaryDirectory() as tmp:
            dump = Path(tmp) / 'image.bin'
            r = self.run_image(data, '--stop-before-entry', '--dump-image', str(dump))
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertEqual(dump.read_bytes(), data[-REPORT_IMAGE_SIZE:])
            bad = Path(tmp) / 'no-such-directory' / 'image.bin'
            r = self.run_image(data, '--dump-image', str(bad))
            self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
            self.assertIn('cannot write image dump', r.stderr)

    def test_stop_before_entry_exits_without_running_the_entry(self):
        # The entry is ud2: running it would fault (exit 132/139), so exit 0 proves it was not entered.
        r = self.run_image(package(b'\x0f\x0b'), '--stop-before-entry')
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertEqual(r.stdout.splitlines()[-1], 'Entering original x86-64 code at guest offset 0x0')
        self.assertNotIn('Guest fault', r.stderr)

    def test_import_report_option_without_path_is_rejected(self):
        r = self.run_image(package(b'\xc3'), '--import-report')
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('Unknown option: --import-report', r.stderr)

    def test_import_report_lists_data_relocations(self):
        # Relocation kind 2 (data): import 0 resolves to a host data symbol, import 1 is a trap.
        data = package(b'\x0f\x0b', [(16, 2, 0, 0), (24, 2, 1, 0)], ['f7uOxY9mM1U#p#J', 'fixture-data'], capabilities=1)
        with tempfile.TemporaryDirectory() as tmp:
            report = Path(tmp) / 'imports.tsv'
            r = self.run_image(data, '--stop-before-entry', '--import-report', str(report))
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            rows = report.read_bytes().decode('ascii').splitlines()
        self.assertEqual(rows, [
            'index\tname\tkind\tlooked_up_as\tresult\tsymbol\tprefix_candidate',
            '0\tf7uOxY9mM1U#p#J\tdata\tf7uOxY9mM1U#p#J\thost\t__stack_chk_guard\t-',
            '1\tfixture-data\tdata\tfixture-data\ttrap\t-\t-',
        ])
        self.assertIn('Imports: 1 host, 0 native, 1 trap, 0 prefix_only', r.stdout.splitlines())

    def test_libc_heap_trace_info_import_resolves_to_a_host_contract(self):
        # libc.prx imports sceLibcHeapGetTraceInfo from libSceLibcInternalExt (a two-part name, never aliased).
        data = package(b'\x0f\x0b', [(16, 1, 0, 0)], ['NWtTN10cJzE#libSceLibcInternalExt'], capabilities=1)
        with tempfile.TemporaryDirectory() as tmp:
            report = Path(tmp) / 'imports.tsv'
            r = self.run_image(data, '--stop-before-entry', '--import-report', str(report))
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            rows = report.read_bytes().decode('ascii').splitlines()
        self.assertEqual(rows[1], '0\tNWtTN10cJzE#libSceLibcInternalExt\tfunc\tNWtTN10cJzE#libSceLibcInternalExt\thost\t'
                                  'sceLibcHeapGetTraceInfo\t-')

    def test_import_report_looked_up_name_is_the_alias_canonical_name(self):
        data = package(b'\x0f\x0b', [(16, 1, 0, 0)], ['fixture-import'], capabilities=1)
        with tempfile.TemporaryDirectory() as tmp:
            report = Path(tmp) / 'imports.tsv'
            aliases = Path(tmp) / 'import-aliases.txt'
            aliases.write_bytes(b'fixture-import uMei1W9uyNo#q#q\n')
            r = self.run_image(data, '--stop-before-entry', '--import-aliases', str(aliases),
                               '--import-report', str(report))
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            rows = report.read_bytes().decode('ascii').splitlines()
        self.assertEqual(rows[1], '0\tfixture-import\tfunc\tuMei1W9uyNo#q#q\thost\texit\t-')
        self.assertIn('Imports: 1 host, 0 native, 0 trap, 0 prefix_only', r.stdout.splitlines())

    def test_import_report_dump_and_stop_flags_are_order_independent(self):
        data = package(b'\x0f\x0b', [(16, 2, 0, 0), (24, 2, 1, 0)], ['f7uOxY9mM1U#p#J', 'fixture-data'], capabilities=1)
        flags = ['--stop-before-entry', '--import-report', '--dump-image']
        expected_rows = None
        with tempfile.TemporaryDirectory() as tmp:
            for order in itertools.permutations(flags):
                with self.subTest(order=order):
                    report, dump = Path(tmp) / 'imports.tsv', Path(tmp) / 'image.bin'
                    report.unlink(missing_ok=True)
                    dump.unlink(missing_ok=True)
                    values = {'--stop-before-entry': [], '--import-report': [str(report)], '--dump-image': [str(dump)]}
                    options = [item for flag in order for item in (flag, *values[flag])]
                    r = self.run_image(data, *options)
                    self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
                    self.assertEqual(r.stdout.splitlines()[-1], 'Entering original x86-64 code at guest offset 0x0')
                    self.assertIn('Imports: 1 host, 0 native, 1 trap, 0 prefix_only', r.stdout.splitlines())
                    self.assertEqual(dump.read_bytes(), data[-4096:])
                    rows = report.read_bytes()
                    expected_rows = expected_rows or rows
                    self.assertEqual(rows, expected_rows)

    def run_user_dir(self, *options, cwd=None, env=None, exe=None):
        # Stop-before-entry run; PES_USER_DIR is removed from the environment unless env sets it.
        with tempfile.TemporaryDirectory() as tmp:
            boot = Path(tmp) / 'boot.bin'
            boot.write_bytes(package(b'\x0f\x0b'))
            environ = {k: v for k, v in os.environ.items() if k != 'PES_USER_DIR'}
            environ.update(env or {})
            return subprocess.run([str(exe or EXE.resolve()), str(boot), '--cpu-only', '--stop-before-entry', *options],
                                  stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=10,
                                  cwd=cwd, env=environ)

    @staticmethod
    def logged(r, prefix):
        lines = [line[len(prefix):] for line in r.stdout.splitlines() if line.startswith(prefix)]
        return lines[0] if len(lines) == 1 else None

    @staticmethod
    def same_path(a, b):
        return os.path.normcase(os.path.abspath(str(a))) == os.path.normcase(os.path.abspath(str(b)))

    def test_default_user_dir_is_repo_user_whatever_the_working_directory(self):
        with tempfile.TemporaryDirectory() as cwd:
            r = self.run_user_dir(cwd=cwd)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        line = self.logged(r, 'User directory: ')
        self.assertIsNotNone(line, r.stdout)
        self.assertTrue(self.same_path(line, ROOT / 'user'), line)

    def test_loader_outside_out_uses_a_user_folder_beside_it(self):
        with tempfile.TemporaryDirectory() as tmp:
            copy = Path(tmp) / 'bin' / EXE.name
            copy.parent.mkdir()
            copy.write_bytes(EXE.read_bytes())
            r = self.run_user_dir(exe=copy.resolve())
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        line = self.logged(r, 'User directory: ')
        self.assertTrue(self.same_path(line, Path(tmp) / 'bin' / 'user'), line)

    def test_user_option_overrides_environment_and_default(self):
        with tempfile.TemporaryDirectory() as tmp:
            user = Path(tmp) / 'chosen'
            r = self.run_user_dir('--user', str(user), env={'PES_USER_DIR': str(Path(tmp) / 'env')})
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertEqual(self.logged(r, 'User directory: '), str(user))

    def test_pes_user_dir_overrides_default(self):
        with tempfile.TemporaryDirectory() as tmp:
            user = Path(tmp) / 'from-env'
            r = self.run_user_dir(env={'PES_USER_DIR': str(user)})
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertEqual(self.logged(r, 'User directory: '), str(user))

    def test_user_dir_inside_app0_is_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            app0 = Path(tmp) / 'app0'
            (app0 / 'sce_sys').mkdir(parents=True)
            r = self.run_user_dir('--app0', str(app0), '--user', str(app0 / 'user'))
            self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
            self.assertIn('inside the game directory', r.stderr)
            self.assertFalse((app0 / 'user').exists())

    def test_user_dir_inside_cusa_parent_of_app0_is_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            game = Path(tmp) / 'CUSA99999'
            (game / 'app0').mkdir(parents=True)
            r = self.run_user_dir('--app0', str(game / 'app0'), '--user', str(game / 'user'))
            self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
            self.assertIn('inside the game directory', r.stderr)
            self.assertFalse((game / 'user').exists())

    def test_user_dir_beside_a_non_cusa_parent_of_app0_is_accepted(self):
        with tempfile.TemporaryDirectory() as tmp:
            app0 = Path(tmp) / 'games' / 'app0'
            app0.mkdir(parents=True)
            r = self.run_user_dir('--app0', str(app0), '--user', str(Path(tmp) / 'games' / 'user'))
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_save_root_is_logged_under_the_user_directory_before_stop(self):
        with tempfile.TemporaryDirectory() as tmp:
            app0 = Path(tmp) / 'game'
            (app0 / 'sce_sys').mkdir(parents=True)
            (app0 / 'sce_sys' / 'param.sfo').write_bytes(sfo_bytes({'TITLE_ID': 'CUSA04957'}))
            user = Path(tmp) / 'user'
            r = self.run_user_dir('--app0', str(app0), '--user', str(user))
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn(f'Save root: {user}/savedata/1/CUSA04957', r.stdout.splitlines())
        self.assertEqual(r.stdout.splitlines()[-1], 'Entering original x86-64 code at guest offset 0x0')


class ProbeGateTests(unittest.TestCase):
    def test_built_loader_is_found_so_the_probe_tests_run(self):
        # LoaderTests is skipped when EXE is missing. Fail if a loader is built under another name
        # (for example the POSIX name on Windows), which would skip the probe suite silently.
        built = [p for p in (ROOT / 'out/pes17recomp', ROOT / 'out/pes17recomp.exe') if p.exists()]
        self.assertTrue(not built or EXE.exists(),
                        f'{[str(p) for p in built]} built, but the probe tests look for {EXE}')


if __name__ == '__main__':
    unittest.main()
