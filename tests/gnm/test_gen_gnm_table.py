# SPDX-License-Identifier: GPL-2.0-or-later
"""scripts/gen_gnm_table.py on a synthetic region listing, callgraph and signature file (our bytes)."""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from paths import ROOT

SCRIPT = ROOT / 'scripts' / 'gen_gnm_table.py'
# f_ok: push rbp / mov rbp,rsp / push rbx = 5 relocatable bytes; no call site.
# f_bad: starts with a RIP-relative lea, so its prologue cannot be moved.
# f_cb: a command buffer method (calls the reserve callback), 7 relocatable bytes.
ASM = (
    'Disassembly of section .data:\n\n'
    '00401000 <.data>:\n'
    '  401000:\t55 \tpush   rbp\n'
    '  401001:\t48 89 e5 \tmov    rbp,rsp\n'
    '  401004:\t53 \tpush   rbx\n'
    '  401005:\tc3 \tret\n'
    '  401040:\t48 8d 05 00 00 00 00 \tlea    rax,[rip+0x0]        # 401047\n'
    '  401047:\tc3 \tret\n'
    '  401060:\t48 8b 07 \tmov    rax,QWORD PTR [rdi]\n'
    '  401063:\t48 83 c0 08 \tadd    rax,0x8\n'
    '  401067:\tff 50 18 \tcall   QWORD PTR [rax+0x18]\n'
    '  40106a:\tc3 \tret\n'
)
CALLGRAPH = (
    '0x00401000 size    32 sites     3 ext_fns     1 int_fns     0\n'
    '0x00401040 size    32 sites     1 ext_fns     1 int_fns     0\n'
    '0x00401060 size    32 sites     0 ext_fns     0 int_fns     0\n'
    '0x00401080 size     8 sites     0 ext_fns     1 int_fns     0\n'
    '0x00401100 size    32 sites     2 ext_fns     0 int_fns     2\n'
    '0x00500000 size    32 sites     4 ext_fns     2 int_fns     0\n'
)
SIGS = '0x00401000 size    32 sites    3 ext    1: SET_CONTEXT_REG(PA_SC_AA_CONFIGx2)\n'
F_OK_ROW = '{0x401000, 5, {0x55, 0x48, 0x89, 0xe5, 0x53}, 3, 0, "gnm_0401000: SET_CONTEXT_REG(PA_SC_AA_CONFIGx2)"},'
F_CB_ROW = '{0x401060, 7, {0x48, 0x8b, 0x07, 0x48, 0x83, 0xc0, 0x08}, 0, 1, "gnm_0401060"},'


class GenGnmTableTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        base = Path(self.tmp.name)
        self.asm = base / 'region.asm'
        self.cg = base / 'cg.txt'
        self.sigs = base / 'sigs.txt'
        self.asm.write_text(ASM)
        self.cg.write_text(CALLGRAPH)
        self.sigs.write_text(SIGS)
        self.out = base / 'out' / 'pes_gnm_table.inc'

    def tearDown(self):
        self.tmp.cleanup()

    def run_gen(self, region='0x401000-0x401100', extra=()):
        args = [sys.executable, str(SCRIPT)]
        if region is not None:
            args += ['--region', region]
        args += [*extra, '-o', str(self.out), str(self.asm), str(self.cg), str(self.sigs)]
        return subprocess.run(args, capture_output=True, text=True)

    def test_region_is_required(self):
        self.assertNotEqual(self.run_gen(region=None).returncode, 0)

    def test_rows_and_skips(self):
        res = self.run_gen()
        self.assertEqual(res.returncode, 0, res.stderr)
        lines = self.out.read_text().splitlines()
        self.assertIn('// 2 functions; 1 skipped (prologue not relocatable).', lines)
        self.assertIn(F_OK_ROW, lines)
        self.assertIn(F_CB_ROW, lines)
        rows = [line for line in lines if line.startswith('{')]
        self.assertEqual(len(rows), 2)

    def test_out_of_region_and_small_rows_dropped(self):
        self.run_gen()
        text = self.out.read_text()
        for absent in ('0x500000', '0x401080', '0x401100', '0x401040'):
            self.assertNotIn('{' + absent + ',', text)

    def test_names_file_overrides_generic_name(self):
        names = Path(self.tmp.name) / 'names.txt'
        names.write_text('# comment\n0x401000 renamed_fn\n')
        res = self.run_gen(extra=['--names', str(names)])
        self.assertEqual(res.returncode, 0, res.stderr)
        self.assertIn('"renamed_fn: SET_CONTEXT_REG(PA_SC_AA_CONFIGx2)"},', self.out.read_text())

    def test_deterministic(self):
        self.assertEqual(self.run_gen().returncode, 0)
        first = self.out.read_bytes()
        self.assertEqual(self.run_gen().returncode, 0)
        self.assertEqual(self.out.read_bytes(), first)

    def test_no_rows_writes_nothing(self):
        res = self.run_gen(region='0x500000-0x500100')
        self.assertNotEqual(res.returncode, 0)
        self.assertFalse(self.out.exists())


if __name__ == '__main__':
    unittest.main()
