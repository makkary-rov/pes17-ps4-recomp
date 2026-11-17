# SPDX-License-Identifier: GPL-2.0-or-later
"""scripts/gnm/pm4_scan_names.py: opcode and register name tables."""
import sys
import unittest

from paths import ROOT

sys.path.insert(0, str(ROOT / 'scripts' / 'gnm'))
import pm4_scan_names as pm4  # noqa: E402


class Pm4NameTests(unittest.TestCase):
    def test_opcode_lookup(self):
        self.assertEqual(pm4.OPCODES[0x69], 'SET_CONTEXT_REG')

    def test_register_lookup(self):
        self.assertEqual(pm4.reg_name('SET_CONTEXT_REG', 0x2f8), 'PA_SC_AA_CONFIG')
        self.assertEqual(pm4.reg_name('SET_SH_REG', 0x48), 'PGM_LO_VS')

    def test_unknown_register_is_hex(self):
        self.assertEqual(pm4.reg_name('SET_UCONFIG_REG', 0x7777), '0x7777')


if __name__ == '__main__':
    unittest.main()
