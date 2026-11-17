# SPDX-License-Identifier: GPL-2.0-or-later
"""scripts/gnm/gnm_sigs.py on a hand-written objdump-style listing (our bytes)."""
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from paths import ROOT

SCRIPT = ROOT / 'scripts' / 'gnm' / 'gnm_sigs.py'
# Function at 0x401000: SET_CONTEXT_REG header (count 2) and its register offset 0x2f8 (PA_SC_AA_CONFIG).
# Function at 0x401010: DRAW_INDEX_AUTO header with count 1 (not a SET_ packet: no register).
LISTING = (
    '  401000:\tc7 00 00 26 69 c0 \tmov    DWORD PTR [rax],0xc0026900\n'
    '  401006:\tc7 40 04 f8 02 00 00 \tmov    DWORD PTR [rax+0x4],0x2f8\n'
    '  401010:\tc7 00 00 2d 01 c0 \tmov    DWORD PTR [rax],0xc0012d00\n'
    '  401016:\tc3 \tret\n'
)
CALLGRAPH = (
    '0x00401000 size    16 sites     0 ext_fns     1 int_fns     0\n'
    '0x00401010 size    16 sites     0 ext_fns     1 int_fns     0\n'
    '0x00401020 size    16 sites     0 ext_fns     0 int_fns     0\n'
)
SIG = re.compile(r'^0x([0-9a-f]+) size\s+\d+ sites\s+\d+ ext\s+\d+: (.*)$')


class GnmSigsTests(unittest.TestCase):
    def run_sigs(self):
        with tempfile.TemporaryDirectory() as tmp:
            asm, cg = Path(tmp) / 'region.asm', Path(tmp) / 'cg.txt'
            asm.write_text(LISTING)
            cg.write_text(CALLGRAPH)
            res = subprocess.run([sys.executable, str(SCRIPT), str(asm), str(cg)],
                                 capture_output=True, text=True)
            self.assertEqual(res.returncode, 0, res.stderr)
            sigs = {}
            for line in res.stdout.splitlines():
                m = SIG.match(line)
                self.assertIsNotNone(m, line)
                sigs[int(m.group(1), 16)] = m.group(2)
            return sigs

    def test_set_context_reg_packet(self):
        sigs = self.run_sigs()
        self.assertEqual(sigs[0x401000], 'SET_CONTEXT_REG(PA_SC_AA_CONFIGx2)')

    def test_non_set_packet_has_no_register(self):
        sigs = self.run_sigs()
        self.assertEqual(sigs[0x401010], 'DRAW_INDEX_AUTO/1')

    def test_function_without_packets_not_listed(self):
        self.assertNotIn(0x401020, self.run_sigs())


if __name__ == '__main__':
    unittest.main()
