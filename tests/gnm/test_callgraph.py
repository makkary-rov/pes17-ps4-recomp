# SPDX-License-Identifier: GPL-2.0-or-later
"""scripts/gnm/callgraph.py on a synthetic ELF64 (our bytes; no title bytes)."""
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from paths import ROOT

SCRIPT = ROOT / 'scripts' / 'gnm' / 'callgraph.py'
TEXT_VA, TEXT_OFF, TEXT_SIZE = 0x401000, 0x200, 0x200
HDR_VA, HDR_OFF = 0x402000, 0x100
PT_LOAD, PT_GNU_EH_FRAME = 1, 0x6474e550
LINE = re.compile(r'^0x([0-9a-f]+) size\s+(\d+) sites\s+(\d+) ext_fns\s+(\d+) int_fns\s+(\d+)')


def build_elf():
    """f0 calls f1, f2 calls f1; f1 is called from f0 and f2. Three 16-byte functions."""
    text = bytearray(TEXT_SIZE)
    text[0x00:0x05] = b'\xe8' + struct.pack('<i', 0x401010 - 0x401005)  # f0: call f1
    text[0x00 + 5] = 0xc3
    text[0x10] = 0xc3  # f1: ret
    text[0x20:0x25] = b'\xe8' + struct.pack('<i', 0x401010 - 0x401025)  # f2: call f1
    text[0x25] = 0xc3
    funcs = [(0x401000, 16), (0x401010, 16), (0x401020, 16)]
    fde_va = [0x401100, 0x401118, 0x401130]
    for va, (start, size) in zip(fde_va, funcs):
        o = va - TEXT_VA
        # length, CIE pointer, pc_begin (unused), pc_range (the function size)
        text[o:o + 20] = struct.pack('<IIiI', 0x14, 0, 0, size) + bytes(4)
    hdr = bytearray(36)
    hdr[0:4] = bytes([1, 0x1b, 0x03, 0x3b])
    struct.pack_into('<I', hdr, 8, len(funcs))
    for i, ((start, _), va) in enumerate(zip(funcs, fde_va)):
        struct.pack_into('<ii', hdr, 12 + 8 * i, start - HDR_VA, va - HDR_VA)
    phdrs = b''.join([
        struct.pack('<IIQQQQQQ', PT_LOAD, 5, TEXT_OFF, TEXT_VA, TEXT_VA, TEXT_SIZE, TEXT_SIZE, 0x1000),
        struct.pack('<IIQQQQQQ', PT_GNU_EH_FRAME, 4, HDR_OFF, HDR_VA, HDR_VA, len(hdr), len(hdr), 4),
    ])
    ident = b'\x7fELF' + bytes([2, 1, 1]) + bytes(9)
    ehdr = ident + struct.pack('<HHIQQQIHHHHHH', 2, 0x3e, 1, 0, 0x40, 0, 0, 64, 56, 2, 64, 0, 0)
    image = bytearray(TEXT_OFF + TEXT_SIZE)
    image[0:len(ehdr)] = ehdr
    image[0x40:0x40 + len(phdrs)] = phdrs
    image[HDR_OFF:HDR_OFF + len(hdr)] = hdr
    image[TEXT_OFF:TEXT_OFF + TEXT_SIZE] = text
    return bytes(image)


class CallgraphTests(unittest.TestCase):
    def run_callgraph(self, lo, hi):
        with tempfile.TemporaryDirectory() as tmp:
            elf = Path(tmp) / 'synthetic.elf'
            out = Path(tmp) / 'cg.txt'
            elf.write_bytes(build_elf())
            res = subprocess.run([sys.executable, str(SCRIPT), str(elf), hex(lo), hex(hi), str(out)],
                                 capture_output=True, text=True)
            self.assertEqual(res.returncode, 0, res.stderr)
            rows = {}
            for line in out.read_text().splitlines():
                m = LINE.match(line)
                self.assertIsNotNone(m, line)
                fn, size, sites, ext, inn = (int(m.group(i), 16 if i == 1 else 10) for i in range(1, 6))
                rows[fn] = {'size': size, 'sites': sites, 'ext': ext, 'int': inn}
            return res.stdout, rows

    def test_whole_range(self):
        stdout, rows = self.run_callgraph(0x401000, 0x401030)
        self.assertIn('functions in range: 3, called from outside: 0', stdout)
        self.assertEqual(rows[0x401000], {'size': 16, 'sites': 0, 'ext': 0, 'int': 0})
        self.assertEqual(rows[0x401010], {'size': 16, 'sites': 2, 'ext': 0, 'int': 2})
        self.assertEqual(rows[0x401020], {'size': 16, 'sites': 0, 'ext': 0, 'int': 0})

    def test_range_cuts_callers(self):
        stdout, rows = self.run_callgraph(0x401010, 0x401030)
        self.assertIn('functions in range: 2, called from outside: 1', stdout)
        self.assertEqual(rows[0x401010], {'size': 16, 'sites': 2, 'ext': 1, 'int': 1})
        self.assertNotIn(0x401000, rows)


if __name__ == '__main__':
    unittest.main()
